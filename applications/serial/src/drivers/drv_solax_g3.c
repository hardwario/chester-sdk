/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_solax_g3.h"
#include "drv_interface.h"
#include "../app_modbus.h"

#include <chester/ctr_rtc.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_solax_g3, LOG_LEVEL_DBG);

/*
 * SolaX X3-Hybrid G3 rejects Modbus reads of more than 2 registers (>=3 -> CRC/-EIO,
 * >=16 -> -ETIMEDOUT), verified on real HW 2026-07-07. Every read below is <= 2 registers:
 * 32-bit LSW values are read as exactly 2 consecutive regs, everything else as 1.
 */

/* Customer registers (FC04, input registers) */
#define REG_PV_POWER          0x000A /* pv1_power, pv2_power (2 regs) */
#define REG_BAT_POWER         0x0016 /* Int16, W */
#define REG_BAT_TEMP          0x0018 /* Int16, degC */
#define REG_BAT_SOC           0x001C /* UInt16, % */
#define REG_FEEDIN_POWER      0x0046 /* Int32 LSW, W */
#define REG_FEEDIN_ENERGY     0x0048 /* UInt32 LSW, x0.01 kWh */
#define REG_CONSUME_ENERGY    0x004A /* UInt32 LSW, x0.01 kWh */
#define REG_EPS_POWER_L1      0x0078 /* UInt16, W */
#define REG_EPS_POWER_L2      0x007C
#define REG_EPS_POWER_L3      0x0080

/* Service registers */
#define REG_SVC_PV_VOLTAGE    0x0003 /* pv1_voltage, pv2_voltage (2 regs) */
#define REG_SVC_PV_CURRENT    0x0005 /* pv1_current, pv2_current (2 regs) */
#define REG_SVC_TEMPERATURE   0x0008 /* Int16, x0.1 degC */
#define REG_SVC_RUN_MODE      0x0009 /* UInt16 */
#define REG_SVC_BAT_VI        0x0014 /* bat_voltage, bat_current (2 regs) */
#define REG_SVC_BMS_STATE     0x0017 /* UInt16 */
#define REG_SVC_ENERGY_OUT    0x001D /* UInt32 LSW, x0.1 kWh */
#define REG_SVC_BMS_WARN_LSB  0x001F /* UInt16 */
#define REG_SVC_ENERGY_TODAY  0x0020 /* UInt16, x0.1 kWh */
#define REG_SVC_BMS_LIMITS    0x0024 /* charge_max, discharge_max (2 regs) */
#define REG_SVC_BMS_WARN_MSB  0x0026 /* UInt16 */
#define REG_SVC_INV_FAULT     0x0040 /* inv_fault_lsb, inv_fault_msb (2 regs) */
#define REG_SVC_MGR_FAULT     0x0043 /* UInt16 */
#define REG_SVC_GRID_L1_VI    0x006A /* voltage_l1, current_l1 (2 regs) */
#define REG_SVC_GRID_L1_PF    0x006C /* power_l1, frequency (2 regs) */
#define REG_SVC_GRID_L2_VI    0x006E /* voltage_l2, current_l2 (2 regs) */
#define REG_SVC_GRID_L2_P     0x0070 /* power_l2 (1 reg) */
#define REG_SVC_GRID_L3_VI    0x0072 /* voltage_l3, current_l3 (2 regs) */
#define REG_SVC_GRID_L3_P     0x0074 /* power_l3 (1 reg) */

static void print_service(const struct shell *shell, struct app_driver_slot *slot);

static int init(struct app_driver_slot *slot)
{
	struct app_data_solax_g3 *data = slot->ctx;

	LOG_INF("Initializing SolaX X3-Hybrid G3 driver");
	data->valid = false;
	data->service_valid = false;
	data->error_count = 0;
	return 0;
}

/* Customer set: periodic, stored in the slot data + record ring */
static int sample(struct app_driver_slot *slot)
{
	struct app_data_solax_g3 *data = slot->ctx;
	uint8_t addr = slot->addr;
	int ret;
	int errors = 0;
	uint16_t r[8];

	float pv1_power = 0, pv2_power = 0, bat_power = 0, bat_temp = 0, bat_soc = 0;
	float feedin_power = 0, feedin_energy = 0, consume_energy = 0;
	float eps_l1 = 0, eps_l2 = 0, eps_l3 = 0;

	LOG_INF("Sampling SolaX G3 at address %d", addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	if (!app_modbus_read_input_regs(addr, REG_PV_POWER, 2, r)) {
		pv1_power = (float)r[0];
		pv2_power = (float)r[1];
	} else {
		errors++;
	}

	if (!app_modbus_read_input_regs(addr, REG_BAT_POWER, 1, r)) {
		bat_power = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	if (!app_modbus_read_input_regs(addr, REG_BAT_TEMP, 1, r)) {
		bat_temp = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	if (!app_modbus_read_input_regs(addr, REG_BAT_SOC, 1, r)) {
		bat_soc = (float)r[0];
	} else {
		errors++;
	}

	/* feedin_power (Int32 W) — read as its own 2-reg pair */
	if (!app_modbus_read_input_regs(addr, REG_FEEDIN_POWER, 2, r)) {
		feedin_power = (float)app_modbus_decode_s32(r, APP_MODBUS_WO_LO_FIRST);
	} else {
		errors++;
	}
	/* feedin_energy_total (UInt32 x0.01 kWh) */
	if (!app_modbus_read_input_regs(addr, REG_FEEDIN_ENERGY, 2, r)) {
		feedin_energy = (float)app_modbus_decode_u32(r, APP_MODBUS_WO_LO_FIRST) * 0.01f;
	} else {
		errors++;
	}
	/* consume_energy_total (UInt32 x0.01 kWh) */
	if (!app_modbus_read_input_regs(addr, REG_CONSUME_ENERGY, 2, r)) {
		consume_energy = (float)app_modbus_decode_u32(r, APP_MODBUS_WO_LO_FIRST) * 0.01f;
	} else {
		errors++;
	}

	if (!app_modbus_read_input_regs(addr, REG_EPS_POWER_L1, 1, r)) {
		eps_l1 = (float)r[0];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_EPS_POWER_L2, 1, r)) {
		eps_l2 = (float)r[0];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_EPS_POWER_L3, 1, r)) {
		eps_l3 = (float)r[0];
	} else {
		errors++;
	}

	app_modbus_release();

	if (errors) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("SolaX G3: %d read errors", errors);
		return -EIO;
	}

	data->pv1_power = pv1_power;
	data->pv2_power = pv2_power;
	data->bat_power = bat_power;
	data->bat_temp = bat_temp;
	data->bat_soc = bat_soc;
	data->feedin_power = feedin_power;
	data->feedin_energy_total = feedin_energy;
	data->consume_energy_total = consume_energy;
	data->eps_power_l1 = eps_l1;
	data->eps_power_l2 = eps_l2;
	data->eps_power_l3 = eps_l3;
	data->valid = true;
	data->last_sample = k_uptime_get_32();
	data->error_count = 0;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct solax_g3_sample rec = {
		.timestamp = ts,
		.pv1_power = pv1_power,
		.pv2_power = pv2_power,
		.bat_power = bat_power,
		.bat_temp = bat_temp,
		.bat_soc = bat_soc,
		.feedin_power = feedin_power,
		.feedin_energy = feedin_energy,
		.consume_energy = consume_energy,
		.eps_power_l1 = eps_l1,
		.eps_power_l2 = eps_l2,
		.eps_power_l3 = eps_l3,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("SolaX G3: PV=%d/%d W, SOC=%d%%, bat=%d W, feedin=%d W", (int)pv1_power,
		(int)pv2_power, (int)bat_soc, (int)bat_power, (int)feedin_power);

	return 0;
}

/* Service set: on demand, stored in the slot data only (not sent to cloud) */
static int sample_service(struct app_driver_slot *slot)
{
	struct app_data_solax_g3 *data = slot->ctx;
	uint8_t addr = slot->addr;
	int ret;
	int errors = 0;
	uint16_t r[16];

	LOG_INF("Sampling SolaX G3 service set at address %d", addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		return ret;
	}

	struct app_data_solax_g3 s;
	memset(&s, 0, sizeof(s));

	/* PV voltage/current (x0.1) */
	if (!app_modbus_read_input_regs(addr, REG_SVC_PV_VOLTAGE, 2, r)) {
		s.pv1_voltage = (float)r[0] * 0.1f;
		s.pv2_voltage = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_PV_CURRENT, 2, r)) {
		s.pv1_current = (float)r[0] * 0.1f;
		s.pv2_current = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	/* temperature (Int16, x0.1 degC — verified on HW: raw 292 = 29.2 C) */
	if (!app_modbus_read_input_regs(addr, REG_SVC_TEMPERATURE, 1, r)) {
		s.temperature = (float)(int16_t)r[0] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_RUN_MODE, 1, r)) {
		s.run_mode = r[0];
	} else {
		errors++;
	}

	/* Battery voltage/current (x0.1) */
	if (!app_modbus_read_input_regs(addr, REG_SVC_BAT_VI, 2, r)) {
		s.bat_voltage = (float)(int16_t)r[0] * 0.1f;
		s.bat_current = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_BMS_STATE, 1, r)) {
		s.bms_state = r[0];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_ENERGY_OUT, 2, r)) {
		s.energy_out_total = (float)app_modbus_decode_u32(r, APP_MODBUS_WO_LO_FIRST) * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_BMS_WARN_LSB, 1, r)) {
		s.bms_warning_lsb = r[0];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_ENERGY_TODAY, 1, r)) {
		s.energy_out_today = (float)r[0] * 0.1f;
	} else {
		errors++;
	}

	/* BMS limits (x0.1 A) */
	if (!app_modbus_read_input_regs(addr, REG_SVC_BMS_LIMITS, 2, r)) {
		s.bms_charge_max = (float)r[0] * 0.1f;
		s.bms_discharge_max = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_BMS_WARN_MSB, 1, r)) {
		s.bms_warning_msb = r[0];
	} else {
		errors++;
	}

	/* Faults */
	if (!app_modbus_read_input_regs(addr, REG_SVC_INV_FAULT, 2, r)) {
		s.inv_fault_lsb = r[0];
		s.inv_fault_msb = r[1];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_MGR_FAULT, 1, r)) {
		s.mgr_fault = r[0];
	} else {
		errors++;
	}

	/* Grid per-phase (voltage x0.1 V, current x0.1 A, power x1 W, freq x0.01 Hz) */
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L1_VI, 2, r)) {
		s.voltage_l1 = (float)r[0] * 0.1f;
		s.current_l1 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L1_PF, 2, r)) {
		s.power_l1 = (float)(int16_t)r[0];
		s.frequency = (float)r[1] * 0.01f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L2_VI, 2, r)) {
		s.voltage_l2 = (float)r[0] * 0.1f;
		s.current_l2 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L2_P, 1, r)) {
		s.power_l2 = (float)(int16_t)r[0];
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L3_VI, 2, r)) {
		s.voltage_l3 = (float)r[0] * 0.1f;
		s.current_l3 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!app_modbus_read_input_regs(addr, REG_SVC_GRID_L3_P, 1, r)) {
		s.power_l3 = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	app_modbus_release();

	if (errors == 0) {
		/* copy service fields into the slot data, keep customer fields/meta */
		data->pv1_voltage = s.pv1_voltage;
		data->pv2_voltage = s.pv2_voltage;
		data->pv1_current = s.pv1_current;
		data->pv2_current = s.pv2_current;
		data->temperature = s.temperature;
		data->run_mode = s.run_mode;
		data->bat_voltage = s.bat_voltage;
		data->bat_current = s.bat_current;
		data->bms_state = s.bms_state;
		data->energy_out_total = s.energy_out_total;
		data->energy_out_today = s.energy_out_today;
		data->bms_warning_lsb = s.bms_warning_lsb;
		data->bms_warning_msb = s.bms_warning_msb;
		data->bms_charge_max = s.bms_charge_max;
		data->bms_discharge_max = s.bms_discharge_max;
		data->inv_fault_lsb = s.inv_fault_lsb;
		data->inv_fault_msb = s.inv_fault_msb;
		data->mgr_fault = s.mgr_fault;
		data->voltage_l1 = s.voltage_l1;
		data->voltage_l2 = s.voltage_l2;
		data->voltage_l3 = s.voltage_l3;
		data->current_l1 = s.current_l1;
		data->current_l2 = s.current_l2;
		data->current_l3 = s.current_l3;
		data->power_l1 = s.power_l1;
		data->power_l2 = s.power_l2;
		data->power_l3 = s.power_l3;
		data->frequency = s.frequency;
		data->service_valid = true;
		return 0;
	}

	data->service_valid = false;
	LOG_WRN("SolaX G3 service: %d read errors", errors);
	return -EIO;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device solax_g3 sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_device_probe_sample(shell, &solax_g3_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int service_op(const struct shell *shell, struct app_driver_slot *slot)
{
	int ret = sample_service(slot);

	if (!ret) {
		print_service(shell, slot);
	}

	return ret;
}

static int cmd_service(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device solax_g3 service <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_device_probe_run(shell, &solax_g3_driver, addr, service_op);
	if (ret) {
		shell_error(shell, "Service sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "SolaX G3: the address is set on the inverter");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	return 0;
}

static const struct shell_static_entry solax_g3_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read customer set: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(service, NULL, "Read service set: service <addr>", cmd_service, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address: config <addr>", cmd_config, 1, 1),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry solax_g3_shell_cmds = {
	.entry = solax_g3_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_solax_g3 *d = slot->ctx;

	app_device_print_header(shell, slot, "SolaX X3-Hybrid G3 (customer)");
	shell_print(shell, "  pv1_power:         %d W", (int)d->pv1_power);
	shell_print(shell, "  pv2_power:         %d W", (int)d->pv2_power);
	shell_print(shell, "  bat_power:         %d W", (int)d->bat_power);
	shell_print(shell, "  bat_temp:          %d C", (int)d->bat_temp);
	shell_print(shell, "  bat_soc:           %d %%", (int)d->bat_soc);
	shell_print(shell, "  feedin_power:      %d W", (int)d->feedin_power);
	shell_print(shell, "  feedin_energy:     %.2f kWh", (double)d->feedin_energy_total);
	shell_print(shell, "  consume_energy:    %.2f kWh", (double)d->consume_energy_total);
	shell_print(shell, "  eps_power_l1:      %d W", (int)d->eps_power_l1);
	shell_print(shell, "  eps_power_l2:      %d W", (int)d->eps_power_l2);
	shell_print(shell, "  eps_power_l3:      %d W", (int)d->eps_power_l3);
}

static void print_service(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_solax_g3 *d = slot->ctx;

	app_device_print_header(shell, slot, "SolaX X3-Hybrid G3 (service)");
	shell_print(shell, "  pv1_voltage:       %.1f V", (double)d->pv1_voltage);
	shell_print(shell, "  pv2_voltage:       %.1f V", (double)d->pv2_voltage);
	shell_print(shell, "  pv1_current:       %.1f A", (double)d->pv1_current);
	shell_print(shell, "  pv2_current:       %.1f A", (double)d->pv2_current);
	shell_print(shell, "  temperature:       %.1f C", (double)d->temperature);
	shell_print(shell, "  run_mode:          %u", d->run_mode);
	shell_print(shell, "  bat_voltage:       %.1f V", (double)d->bat_voltage);
	shell_print(shell, "  bat_current:       %.1f A", (double)d->bat_current);
	shell_print(shell, "  bms_state:         %u", d->bms_state);
	shell_print(shell, "  energy_out_total:  %.1f kWh", (double)d->energy_out_total);
	shell_print(shell, "  energy_out_today:  %.1f kWh", (double)d->energy_out_today);
	shell_print(shell, "  bms_warning:       0x%04x%04x", d->bms_warning_msb,
		    d->bms_warning_lsb);
	shell_print(shell, "  bms_charge_max:    %.1f A", (double)d->bms_charge_max);
	shell_print(shell, "  bms_discharge_max: %.1f A", (double)d->bms_discharge_max);
	shell_print(shell, "  inv_fault:         0x%04x%04x", d->inv_fault_msb, d->inv_fault_lsb);
	shell_print(shell, "  mgr_fault:         0x%04x", d->mgr_fault);
	shell_print(shell, "  voltage_l1/l2/l3:  %.1f / %.1f / %.1f V", (double)d->voltage_l1,
		    (double)d->voltage_l2, (double)d->voltage_l3);
	shell_print(shell, "  current_l1/l2/l3:  %.1f / %.1f / %.1f A", (double)d->current_l1,
		    (double)d->current_l2, (double)d->current_l3);
	shell_print(shell, "  power_l1/l2/l3:    %d / %d / %d W", (int)d->power_l1,
		    (int)d->power_l2, (int)d->power_l3);
	shell_print(shell, "  frequency:         %.2f Hz", (double)d->frequency);
}

const struct app_device_driver solax_g3_driver = {
	.name = "solax_g3",
	.type = APP_DEVICE_TYPE_SOLAX_G3,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_solax_g3),
	.rec_size = sizeof(struct solax_g3_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_solax_g3 *solax_g3_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
