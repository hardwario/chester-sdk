/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_iem3000.h"
#include "drv_interface.h"
#include "../app_modbus.h"

#include <chester/ctr_rtc.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_iem3000, LOG_LEVEL_DBG);

/* iEM3000 Register addresses (Float32 word-swap, INT64 for energy) */
#define REG_CURRENT_L1        0x0BB7
#define REG_CURRENT_L2        0x0BB9
#define REG_CURRENT_L3        0x0BBB
#define REG_CURRENT           0x0BC1
#define REG_VOLTAGE_L1        0x0BD3
#define REG_VOLTAGE_L2        0x0BD5
#define REG_VOLTAGE_L3        0x0BD7
#define REG_POWER_L1          0x0BED
#define REG_POWER_L2          0x0BEF
#define REG_POWER_L3          0x0BF1
#define REG_POWER             0x0BF3
#define REG_POWER_REACTIVE    0x0BFB
#define REG_POWER_APPARENT    0x0C03
#define REG_POWER_FACTOR      0x0C0B
#define REG_FREQUENCY         0x0C25
#define REG_ENERGY_IN         0x0C83  /* INT64, /1000 kWh */
#define REG_ENERGY_OUT        0x0C87  /* INT64, /1000 kWh */
#define REG_ENERGY_REACTIVE_IN  0x0C93
#define REG_ENERGY_REACTIVE_OUT 0x0C97

/* clang-format off */
/* Float32 values (energy import / export is INT64, see read_energy) */
static const struct app_device_reg m_regs[] = {
	APP_DEVICE_REG(struct app_data_iem3000, current_l1, REG_CURRENT_L1, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, current_l2, REG_CURRENT_L2, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, current_l3, REG_CURRENT_L3, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, current, REG_CURRENT, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, voltage_l1, REG_VOLTAGE_L1, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, voltage_l2, REG_VOLTAGE_L2, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, voltage_l3, REG_VOLTAGE_L3, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_l1, REG_POWER_L1, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_l2, REG_POWER_L2, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_l3, REG_POWER_L3, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power, REG_POWER, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_reactive, REG_POWER_REACTIVE, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_apparent, REG_POWER_APPARENT, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, power_factor, REG_POWER_FACTOR, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, frequency, REG_FREQUENCY, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, energy_reactive_in, REG_ENERGY_REACTIVE_IN, APP_MODBUS_FLOAT_LO, 1.0f),
	APP_DEVICE_REG(struct app_data_iem3000, energy_reactive_out, REG_ENERGY_REACTIVE_OUT, APP_MODBUS_FLOAT_LO, 1.0f),
};
/* clang-format on */

/* Energy: INT64 high word first, Wh; read as two 2-register transactions */
static int read_energy(uint8_t addr, uint16_t reg, float *out)
{
	uint16_t regs[4];
	int ret;

	ret = app_modbus_read_holding_regs(addr, reg, 2, &regs[0]);
	if (ret) {
		return ret;
	}

	ret = app_modbus_read_holding_regs(addr, reg + 2, 2, &regs[2]);
	if (ret) {
		return ret;
	}

	*out = (float)app_modbus_decode_s64(regs, APP_MODBUS_WO_HI_FIRST) / 1000.0f;
	return 0;
}

static int init(struct app_driver_slot *slot)
{
	struct app_data_iem3000 *data = slot->ctx;

	LOG_INF("Initializing iEM3000 driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_iem3000 *data = slot->ctx;
	struct app_data_iem3000 values = *data;
	int errors = 0;
	int ret;

	LOG_INF("Sampling iEM3000 at address %d", slot->addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	/* All values are read; the data is updated only if every read succeeded */
	ret = app_device_read_regs(slot->addr, APP_MODBUS_FC_HOLDING, m_regs, ARRAY_SIZE(m_regs),
				   &values, false);
	if (ret) {
		errors++;
	}

	/* The meter does not answer: the energy reads would only wait for the timeout too */
	if (ret != -ETIMEDOUT) {
		if (read_energy(slot->addr, REG_ENERGY_IN, &values.energy_in)) {
			errors++;
		}

		if (read_energy(slot->addr, REG_ENERGY_OUT, &values.energy_out)) {
			errors++;
		}
	}

	app_modbus_release();

	if (errors) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("iEM3000: read errors");
		return -EIO;
	}

	values.valid = true;
	values.last_sample = k_uptime_get_32();
	values.error_count = 0;
	*data = values;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct iem3000_sample rec = {
		.timestamp = ts,
		.voltage_l1 = data->voltage_l1,
		.voltage_l2 = data->voltage_l2,
		.voltage_l3 = data->voltage_l3,
		.current_l1 = data->current_l1,
		.current_l2 = data->current_l2,
		.current_l3 = data->current_l3,
		.power_l1 = data->power_l1,
		.power_l2 = data->power_l2,
		.power_l3 = data->power_l3,
		.power = data->power,
		.power_apparent = data->power_apparent,
		.power_reactive = data->power_reactive,
		.power_factor = data->power_factor,
		.frequency = data->frequency,
		.energy_in = data->energy_in,
		.energy_out = data->energy_out,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("iEM3000: P=%.1f W, E_in=%.1f kWh, PF=%.2f", (double)data->power,
		(double)data->energy_in, (double)data->power_factor);

	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device iem3000 sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_device_probe_sample(shell, &iem3000_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "iEM3000: address and parity are set on the meter or with its software");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	shell_print(shell, "Set the matching parity: app config serial-parity");
	return 0;
}

static const struct shell_static_entry iem3000_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address and parity", cmd_config, 1, 2),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry iem3000_shell_cmds = {
	.entry = iem3000_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_iem3000 *data = slot->ctx;

	app_device_print_header(shell, slot, "iEM3000");
	shell_print(shell, "  voltage_l1:     %.1f V", (double)data->voltage_l1);
	shell_print(shell, "  voltage_l2:     %.1f V", (double)data->voltage_l2);
	shell_print(shell, "  voltage_l3:     %.1f V", (double)data->voltage_l3);
	shell_print(shell, "  current_l1:     %.3f A", (double)data->current_l1);
	shell_print(shell, "  current_l2:     %.3f A", (double)data->current_l2);
	shell_print(shell, "  current_l3:     %.3f A", (double)data->current_l3);
	shell_print(shell, "  frequency:      %.2f Hz", (double)data->frequency);
	shell_print(shell, "  power_l1:       %.4f kW", (double)data->power_l1);
	shell_print(shell, "  power_l2:       %.4f kW", (double)data->power_l2);
	shell_print(shell, "  power_l3:       %.4f kW", (double)data->power_l3);
	shell_print(shell, "  power:          %.4f kW", (double)data->power);
	shell_print(shell, "  power_apparent: %.4f kVA", (double)data->power_apparent);
	shell_print(shell, "  power_reactive: %.4f kvar", (double)data->power_reactive);
	shell_print(shell, "  power_factor:   %.3f", (double)data->power_factor);
	shell_print(shell, "  energy_in:      %.3f kWh", (double)data->energy_in);
	shell_print(shell, "  energy_out:     %.3f kWh", (double)data->energy_out);
}

const struct app_device_driver iem3000_driver = {
	.name = "iem3000",
	.type = APP_DEVICE_TYPE_IEM3000,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_iem3000),
	.rec_size = sizeof(struct iem3000_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_iem3000 *iem3000_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
