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

#define MAX_SAMPLES 32
#define DEFAULT_ADDR 1

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

/* LSW-first 32-bit decode from a locally read block */
static inline uint32_t u32_lsw(const uint16_t *r, int i)
{
	return (uint32_t)r[i] | ((uint32_t)r[i + 1] << 16);
}
static inline int32_t s32_lsw(const uint16_t *r, int i)
{
	return (int32_t)u32_lsw(r, i);
}

static struct app_data_solax_g3 m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
	.service_valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

static struct solax_g3_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

static int read_input(uint8_t addr, uint16_t reg, uint16_t count, uint16_t *regs)
{
	return app_modbus_read_input_regs(addr, reg, count, regs);
}

static void print_data(const struct shell *shell, int idx, int addr);
static void print_service(const struct shell *shell, int addr);

static int init(void)
{
	LOG_INF("Initializing SolaX X3-Hybrid G3 driver");
	m_data.valid = false;
	m_data.service_valid = false;
	m_data.error_count = 0;
	return 0;
}

/* Customer set: periodic, stored in struct + ring buffer */
static int sample(void)
{
	int ret;
	int errors = 0;
	uint8_t addr;
	uint16_t r[8];

	float pv1_power = 0, pv2_power = 0, bat_power = 0, bat_temp = 0, bat_soc = 0;
	float feedin_power = 0, feedin_energy = 0, consume_energy = 0;
	float eps_l1 = 0, eps_l2 = 0, eps_l3 = 0;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling SolaX G3 at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	if (!read_input(addr, REG_PV_POWER, 2, r)) {
		pv1_power = (float)r[0];
		pv2_power = (float)r[1];
	} else {
		errors++;
	}

	if (!read_input(addr, REG_BAT_POWER, 1, r)) {
		bat_power = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	if (!read_input(addr, REG_BAT_TEMP, 1, r)) {
		bat_temp = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	if (!read_input(addr, REG_BAT_SOC, 1, r)) {
		bat_soc = (float)r[0];
	} else {
		errors++;
	}

	/* feedin_power (Int32 W) — read as its own 2-reg pair */
	if (!read_input(addr, REG_FEEDIN_POWER, 2, r)) {
		feedin_power = (float)s32_lsw(r, 0);
	} else {
		errors++;
	}
	/* feedin_energy_total (UInt32 x0.01 kWh) */
	if (!read_input(addr, REG_FEEDIN_ENERGY, 2, r)) {
		feedin_energy = (float)u32_lsw(r, 0) * 0.01f;
	} else {
		errors++;
	}
	/* consume_energy_total (UInt32 x0.01 kWh) */
	if (!read_input(addr, REG_CONSUME_ENERGY, 2, r)) {
		consume_energy = (float)u32_lsw(r, 0) * 0.01f;
	} else {
		errors++;
	}

	if (!read_input(addr, REG_EPS_POWER_L1, 1, r)) {
		eps_l1 = (float)r[0];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_EPS_POWER_L2, 1, r)) {
		eps_l2 = (float)r[0];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_EPS_POWER_L3, 1, r)) {
		eps_l3 = (float)r[0];
	} else {
		errors++;
	}

	app_modbus_disable();

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.pv1_power = pv1_power;
		m_data.pv2_power = pv2_power;
		m_data.bat_power = bat_power;
		m_data.bat_temp = bat_temp;
		m_data.bat_soc = bat_soc;
		m_data.feedin_power = feedin_power;
		m_data.feedin_energy_total = feedin_energy;
		m_data.consume_energy_total = consume_energy;
		m_data.eps_power_l1 = eps_l1;
		m_data.eps_power_l2 = eps_l2;
		m_data.eps_power_l3 = eps_l3;
		m_data.valid = true;
		m_data.last_sample = k_uptime_get_32();
		m_data.error_count = 0;
		k_mutex_unlock(&m_data_mutex);

		k_mutex_lock(&m_samples_mutex, K_FOREVER);
		if (m_sample_count < MAX_SAMPLES) {
			uint64_t ts;
			ctr_rtc_get_ts(&ts);

			m_samples[m_sample_count].timestamp = ts;
			m_samples[m_sample_count].pv1_power = pv1_power;
			m_samples[m_sample_count].pv2_power = pv2_power;
			m_samples[m_sample_count].bat_power = bat_power;
			m_samples[m_sample_count].bat_temp = bat_temp;
			m_samples[m_sample_count].bat_soc = bat_soc;
			m_samples[m_sample_count].feedin_power = feedin_power;
			m_samples[m_sample_count].feedin_energy = feedin_energy;
			m_samples[m_sample_count].consume_energy = consume_energy;
			m_samples[m_sample_count].eps_power_l1 = eps_l1;
			m_samples[m_sample_count].eps_power_l2 = eps_l2;
			m_samples[m_sample_count].eps_power_l3 = eps_l3;
			m_sample_count++;
			LOG_DBG("SolaX G3: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("SolaX G3: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("SolaX G3: PV=%d/%d W, SOC=%d%%, bat=%d W, feedin=%d W",
			(int)pv1_power, (int)pv2_power, (int)bat_soc, (int)bat_power,
			(int)feedin_power);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("SolaX G3: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

/* Service set: on demand, stored in struct only (not sent to cloud) */
int solax_g3_sample_service(void)
{
	int ret;
	int errors = 0;
	uint8_t addr;
	uint16_t r[16];

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling SolaX G3 service set at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		return ret;
	}

	struct app_data_solax_g3 s;
	memset(&s, 0, sizeof(s));

	/* PV voltage/current (x0.1) */
	if (!read_input(addr, REG_SVC_PV_VOLTAGE, 2, r)) {
		s.pv1_voltage = (float)r[0] * 0.1f;
		s.pv2_voltage = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_PV_CURRENT, 2, r)) {
		s.pv1_current = (float)r[0] * 0.1f;
		s.pv2_current = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	/* temperature (Int16, x0.1 degC — verified on HW: raw 292 = 29.2 C) */
	if (!read_input(addr, REG_SVC_TEMPERATURE, 1, r)) {
		s.temperature = (float)(int16_t)r[0] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_RUN_MODE, 1, r)) {
		s.run_mode = r[0];
	} else {
		errors++;
	}

	/* Battery voltage/current (x0.1) */
	if (!read_input(addr, REG_SVC_BAT_VI, 2, r)) {
		s.bat_voltage = (float)(int16_t)r[0] * 0.1f;
		s.bat_current = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_BMS_STATE, 1, r)) {
		s.bms_state = r[0];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_ENERGY_OUT, 2, r)) {
		s.energy_out_total = (float)u32_lsw(r, 0) * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_BMS_WARN_LSB, 1, r)) {
		s.bms_warning_lsb = r[0];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_ENERGY_TODAY, 1, r)) {
		s.energy_out_today = (float)r[0] * 0.1f;
	} else {
		errors++;
	}

	/* BMS limits (x0.1 A) */
	if (!read_input(addr, REG_SVC_BMS_LIMITS, 2, r)) {
		s.bms_charge_max = (float)r[0] * 0.1f;
		s.bms_discharge_max = (float)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_BMS_WARN_MSB, 1, r)) {
		s.bms_warning_msb = r[0];
	} else {
		errors++;
	}

	/* Faults */
	if (!read_input(addr, REG_SVC_INV_FAULT, 2, r)) {
		s.inv_fault_lsb = r[0];
		s.inv_fault_msb = r[1];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_MGR_FAULT, 1, r)) {
		s.mgr_fault = r[0];
	} else {
		errors++;
	}

	/* Grid per-phase (voltage x0.1 V, current x0.1 A, power x1 W, freq x0.01 Hz) */
	if (!read_input(addr, REG_SVC_GRID_L1_VI, 2, r)) {
		s.voltage_l1 = (float)r[0] * 0.1f;
		s.current_l1 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_GRID_L1_PF, 2, r)) {
		s.power_l1 = (float)(int16_t)r[0];
		s.frequency = (float)r[1] * 0.01f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_GRID_L2_VI, 2, r)) {
		s.voltage_l2 = (float)r[0] * 0.1f;
		s.current_l2 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_GRID_L2_P, 1, r)) {
		s.power_l2 = (float)(int16_t)r[0];
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_GRID_L3_VI, 2, r)) {
		s.voltage_l3 = (float)r[0] * 0.1f;
		s.current_l3 = (float)(int16_t)r[1] * 0.1f;
	} else {
		errors++;
	}
	if (!read_input(addr, REG_SVC_GRID_L3_P, 1, r)) {
		s.power_l3 = (float)(int16_t)r[0];
	} else {
		errors++;
	}

	app_modbus_disable();

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		/* copy service fields into m_data, keep customer fields/meta */
		m_data.pv1_voltage = s.pv1_voltage;
		m_data.pv2_voltage = s.pv2_voltage;
		m_data.pv1_current = s.pv1_current;
		m_data.pv2_current = s.pv2_current;
		m_data.temperature = s.temperature;
		m_data.run_mode = s.run_mode;
		m_data.bat_voltage = s.bat_voltage;
		m_data.bat_current = s.bat_current;
		m_data.bms_state = s.bms_state;
		m_data.energy_out_total = s.energy_out_total;
		m_data.energy_out_today = s.energy_out_today;
		m_data.bms_warning_lsb = s.bms_warning_lsb;
		m_data.bms_warning_msb = s.bms_warning_msb;
		m_data.bms_charge_max = s.bms_charge_max;
		m_data.bms_discharge_max = s.bms_discharge_max;
		m_data.inv_fault_lsb = s.inv_fault_lsb;
		m_data.inv_fault_msb = s.inv_fault_msb;
		m_data.mgr_fault = s.mgr_fault;
		m_data.voltage_l1 = s.voltage_l1;
		m_data.voltage_l2 = s.voltage_l2;
		m_data.voltage_l3 = s.voltage_l3;
		m_data.current_l1 = s.current_l1;
		m_data.current_l2 = s.current_l2;
		m_data.current_l3 = s.current_l3;
		m_data.power_l1 = s.power_l1;
		m_data.power_l2 = s.power_l2;
		m_data.power_l3 = s.power_l3;
		m_data.frequency = s.frequency;
		m_data.service_valid = true;
		k_mutex_unlock(&m_data_mutex);
		return 0;
	}

	m_data.service_valid = false;
	k_mutex_unlock(&m_data_mutex);
	LOG_WRN("SolaX G3 service: %d read errors", errors);
	return -EIO;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	ARG_UNUSED(parity_str);
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);
	LOG_INF("SolaX G3: sampling address set to %d", new_addr);
	return 0;
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

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t old_addr = m_data.modbus_addr;
	m_data.modbus_addr = addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = sample();

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = old_addr;
	k_mutex_unlock(&m_data_mutex);

	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	print_data(shell, 0, addr);
	return 0;
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

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t old_addr = m_data.modbus_addr;
	m_data.modbus_addr = addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = solax_g3_sample_service();

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = old_addr;
	k_mutex_unlock(&m_data_mutex);

	if (ret) {
		shell_error(shell, "Service sampling failed: %d", ret);
		return ret;
	}

	print_service(shell, addr);
	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device solax_g3 config <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	config(addr, NULL);
	shell_print(shell, "Sampling address set to %d", addr);
	return 0;
}

static const struct shell_static_entry solax_g3_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read customer set: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(service, NULL, "Read service set: service <addr>", cmd_service, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Set sampling address: config <addr>", cmd_config, 2, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry solax_g3_shell_cmds = {
	.entry = solax_g3_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_solax_g3 d = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] SolaX X3-Hybrid G3 @ addr %d (customer):", idx, addr);
	shell_print(shell, "  pv1_power:         %d W", (int)d.pv1_power);
	shell_print(shell, "  pv2_power:         %d W", (int)d.pv2_power);
	shell_print(shell, "  bat_power:         %d W", (int)d.bat_power);
	shell_print(shell, "  bat_temp:          %d C", (int)d.bat_temp);
	shell_print(shell, "  bat_soc:           %d %%", (int)d.bat_soc);
	shell_print(shell, "  feedin_power:      %d W", (int)d.feedin_power);
	shell_print(shell, "  feedin_energy:     %.2f kWh", (double)d.feedin_energy_total);
	shell_print(shell, "  consume_energy:    %.2f kWh", (double)d.consume_energy_total);
	shell_print(shell, "  eps_power_l1:      %d W", (int)d.eps_power_l1);
	shell_print(shell, "  eps_power_l2:      %d W", (int)d.eps_power_l2);
	shell_print(shell, "  eps_power_l3:      %d W", (int)d.eps_power_l3);
}

static void print_service(const struct shell *shell, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_solax_g3 d = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "SolaX X3-Hybrid G3 @ addr %d (service):", addr);
	shell_print(shell, "  pv1_voltage:       %.1f V", (double)d.pv1_voltage);
	shell_print(shell, "  pv2_voltage:       %.1f V", (double)d.pv2_voltage);
	shell_print(shell, "  pv1_current:       %.1f A", (double)d.pv1_current);
	shell_print(shell, "  pv2_current:       %.1f A", (double)d.pv2_current);
	shell_print(shell, "  temperature:       %.1f C", (double)d.temperature);
	shell_print(shell, "  run_mode:          %u", d.run_mode);
	shell_print(shell, "  bat_voltage:       %.1f V", (double)d.bat_voltage);
	shell_print(shell, "  bat_current:       %.1f A", (double)d.bat_current);
	shell_print(shell, "  bms_state:         %u", d.bms_state);
	shell_print(shell, "  energy_out_total:  %.1f kWh", (double)d.energy_out_total);
	shell_print(shell, "  energy_out_today:  %.1f kWh", (double)d.energy_out_today);
	shell_print(shell, "  bms_warning:       0x%04x%04x", d.bms_warning_msb, d.bms_warning_lsb);
	shell_print(shell, "  bms_charge_max:    %.1f A", (double)d.bms_charge_max);
	shell_print(shell, "  bms_discharge_max: %.1f A", (double)d.bms_discharge_max);
	shell_print(shell, "  inv_fault:         0x%04x%04x", d.inv_fault_msb, d.inv_fault_lsb);
	shell_print(shell, "  mgr_fault:         0x%04x", d.mgr_fault);
	shell_print(shell, "  voltage_l1/l2/l3:  %.1f / %.1f / %.1f V", (double)d.voltage_l1,
		    (double)d.voltage_l2, (double)d.voltage_l3);
	shell_print(shell, "  current_l1/l2/l3:  %.1f / %.1f / %.1f A", (double)d.current_l1,
		    (double)d.current_l2, (double)d.current_l3);
	shell_print(shell, "  power_l1/l2/l3:    %d / %d / %d W", (int)d.power_l1, (int)d.power_l2,
		    (int)d.power_l3);
	shell_print(shell, "  frequency:         %.2f Hz", (double)d.frequency);
}

const struct app_device_driver solax_g3_driver = {
	.name = "solax_g3",
	.type = APP_DEVICE_TYPE_SOLAX_G3,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_solax_g3 *solax_g3_get_data(void)
{
	return &m_data;
}

void solax_g3_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t solax_g3_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int solax_g3_get_samples(struct solax_g3_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void solax_g3_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("SolaX G3: Sample buffer cleared");
}

int solax_g3_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
