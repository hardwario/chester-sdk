/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_or_we_516.h"
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

LOG_MODULE_REGISTER(drv_or_we_516, LOG_LEVEL_DBG);

/* OR-WE-516 Register addresses (Float32 BE) */
#define REG_VOLTAGE_L1        0x000E
#define REG_VOLTAGE_L2        0x0010
#define REG_VOLTAGE_L3        0x0012
#define REG_FREQUENCY         0x0014
#define REG_CURRENT_L1        0x0016
#define REG_CURRENT_L2        0x0018
#define REG_CURRENT_L3        0x001A
#define REG_POWER             0x001C
#define REG_POWER_L1          0x001E
#define REG_POWER_L2          0x0020
#define REG_POWER_L3          0x0022
#define REG_POWER_REACTIVE    0x0024
#define REG_POWER_APPARENT    0x002C
#define REG_POWER_FACTOR      0x0034
#define REG_PF_L1             0x0036  /* Float32 BE, PF L1 */
#define REG_PF_L2             0x0038  /* Float32 BE, PF L2 */
#define REG_PF_L3             0x003A  /* Float32 BE, PF L3 */
#define REG_ENERGY            0x0100
#define REG_ENERGY_L1         0x0102
#define REG_ENERGY_L2         0x0104
#define REG_ENERGY_L3         0x0106
#define REG_ENERGY_IN         0x0108
#define REG_ENERGY_OUT        0x0110
#define REG_ENERGY_REACTIVE   0x0118
#define REG_ENERGY_REACTIVE_IN  0x0120
#define REG_ENERGY_REACTIVE_OUT 0x0128

/* clang-format off */
static const struct app_device_reg m_regs[] = {
	APP_DEVICE_REG(struct app_data_or_we_516, voltage_l1, REG_VOLTAGE_L1, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, voltage_l2, REG_VOLTAGE_L2, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, voltage_l3, REG_VOLTAGE_L3, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, frequency, REG_FREQUENCY, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, current_l1, REG_CURRENT_L1, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, current_l2, REG_CURRENT_L2, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, current_l3, REG_CURRENT_L3, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power, REG_POWER, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_l1, REG_POWER_L1, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_l2, REG_POWER_L2, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_l3, REG_POWER_L3, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_reactive, REG_POWER_REACTIVE, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_apparent, REG_POWER_APPARENT, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_factor, REG_POWER_FACTOR, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_factor_l1, REG_PF_L1, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_factor_l2, REG_PF_L2, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, power_factor_l3, REG_PF_L3, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy, REG_ENERGY, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_l1, REG_ENERGY_L1, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_l2, REG_ENERGY_L2, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_l3, REG_ENERGY_L3, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_in, REG_ENERGY_IN, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_out, REG_ENERGY_OUT, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_reactive, REG_ENERGY_REACTIVE, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_reactive_in, REG_ENERGY_REACTIVE_IN, APP_MODBUS_FLOAT_HI, 1.0f),
	APP_DEVICE_REG(struct app_data_or_we_516, energy_reactive_out, REG_ENERGY_REACTIVE_OUT, APP_MODBUS_FLOAT_HI, 1.0f),
};
/* clang-format on */

static int init(struct app_driver_slot *slot)
{
	struct app_data_or_we_516 *data = slot->ctx;

	LOG_INF("Initializing OR-WE-516 driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_or_we_516 *data = slot->ctx;
	struct app_data_or_we_516 values = *data;
	int ret;

	LOG_INF("Sampling OR-WE-516 at address %d", slot->addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	/* All values are read; the data is updated only if every read succeeded */
	ret = app_device_read_regs(slot->addr, APP_MODBUS_FC_HOLDING, m_regs, ARRAY_SIZE(m_regs),
				   &values, false);

	app_modbus_release();

	if (ret) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("OR-WE-516: read errors");
		return -EIO;
	}

	values.valid = true;
	values.last_sample = k_uptime_get_32();
	values.error_count = 0;
	*data = values;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct or_we_516_sample rec = {
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
		.power_factor_l1 = data->power_factor_l1,
		.power_factor_l2 = data->power_factor_l2,
		.power_factor_l3 = data->power_factor_l3,
		.frequency = data->frequency,
		.energy = data->energy,
		.energy_in = data->energy_in,
		.energy_out = data->energy_out,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("OR-WE-516: P=%.2f kW, E=%.1f kWh, PF=%.2f", (double)data->power,
		(double)data->energy, (double)data->power_factor);

	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device or_we_516 sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_device_probe_sample(shell, &or_we_516_driver, addr);
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

	shell_print(shell, "OR-WE-516: the address is set by DIP switches, parity is fixed to 8E1");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	return 0;
}

static const struct shell_static_entry or_we_516_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address: config <addr>", cmd_config, 1, 1),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry or_we_516_shell_cmds = {
	.entry = or_we_516_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_or_we_516 *data = slot->ctx;

	app_device_print_header(shell, slot, "OR-WE-516");
	shell_print(shell, "  voltage_l1:        %.1f V", (double)data->voltage_l1);
	shell_print(shell, "  voltage_l2:        %.1f V", (double)data->voltage_l2);
	shell_print(shell, "  voltage_l3:        %.1f V", (double)data->voltage_l3);
	shell_print(shell, "  current_l1:        %.3f A", (double)data->current_l1);
	shell_print(shell, "  current_l2:        %.3f A", (double)data->current_l2);
	shell_print(shell, "  current_l3:        %.3f A", (double)data->current_l3);
	shell_print(shell, "  frequency:         %.2f Hz", (double)data->frequency);
	shell_print(shell, "  power_l1:          %.4f kW", (double)data->power_l1);
	shell_print(shell, "  power_l2:          %.4f kW", (double)data->power_l2);
	shell_print(shell, "  power_l3:          %.4f kW", (double)data->power_l3);
	shell_print(shell, "  power:             %.4f kW", (double)data->power);
	shell_print(shell, "  power_apparent:    %.4f kVA", (double)data->power_apparent);
	shell_print(shell, "  power_reactive:    %.4f kvar", (double)data->power_reactive);
	shell_print(shell, "  power_factor:      %.3f", (double)data->power_factor);
	shell_print(shell, "  power_factor_l1:   %.3f", (double)data->power_factor_l1);
	shell_print(shell, "  power_factor_l2:   %.3f", (double)data->power_factor_l2);
	shell_print(shell, "  power_factor_l3:   %.3f", (double)data->power_factor_l3);
	shell_print(shell, "  energy:            %.3f kWh", (double)data->energy);
	shell_print(shell, "  energy_in:         %.3f kWh", (double)data->energy_in);
	shell_print(shell, "  energy_out:        %.3f kWh", (double)data->energy_out);
}

const struct app_device_driver or_we_516_driver = {
	.name = "or_we_516",
	.type = APP_DEVICE_TYPE_OR_WE_516,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_or_we_516),
	.rec_size = sizeof(struct or_we_516_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_or_we_516 *or_we_516_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
