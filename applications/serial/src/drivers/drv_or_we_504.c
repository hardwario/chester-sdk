/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_or_we_504.h"
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

LOG_MODULE_REGISTER(drv_or_we_504, LOG_LEVEL_DBG);

/* OR-WE-504 Register addresses */
#define REG_VOLTAGE       0x0000  /* UINT16, /10 V */
#define REG_CURRENT       0x0001  /* UINT16, /10 A */
#define REG_FREQUENCY     0x0002  /* UINT16, /10 Hz */
#define REG_POWER         0x0003  /* INT16,  W (signed) */
#define REG_REACTIVE      0x0004  /* INT16,  var (signed) */
#define REG_APPARENT      0x0005  /* UINT16, VA */
#define REG_PF            0x0006  /* INT16,  /1000 PF */
#define REG_ENERGY        0x0007  /* UINT32 BE, Wh */
#define REG_ENERGY_REACT  0x0009  /* UINT32 BE, varh */

/* Driver functions */

static int init(struct app_driver_slot *slot)
{
	struct app_data_or_we_504 *data = slot->ctx;

	LOG_INF("Initializing OR-WE-504 driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

/* clang-format off */
static const struct app_device_reg m_regs[] = {
	APP_DEVICE_REG(struct app_data_or_we_504, voltage, REG_VOLTAGE, APP_MODBUS_U16, 10.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, current, REG_CURRENT, APP_MODBUS_U16, 10.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, frequency, REG_FREQUENCY, APP_MODBUS_U16, 10.0f),
	/* W → kW, var → kvar, VA → kVA */
	APP_DEVICE_REG(struct app_data_or_we_504, power, REG_POWER, APP_MODBUS_S16, 1000.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, power_reactive, REG_REACTIVE, APP_MODBUS_S16, 1000.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, power_apparent, REG_APPARENT, APP_MODBUS_U16, 1000.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, power_factor, REG_PF, APP_MODBUS_S16, 1000.0f),
	/* Wh → kWh, varh → kvarh */
	APP_DEVICE_REG(struct app_data_or_we_504, energy_in, REG_ENERGY, APP_MODBUS_U32_HI, 1000.0f),
	APP_DEVICE_REG(struct app_data_or_we_504, energy_reactive, REG_ENERGY_REACT, APP_MODBUS_U32_HI, 1000.0f),
};
/* clang-format on */

static int sample(struct app_driver_slot *slot)
{
	struct app_data_or_we_504 *data = slot->ctx;
	struct app_data_or_we_504 values = *data;
	int ret;

	LOG_INF("Sampling OR-WE-504 at address %d", slot->addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	ret = app_device_read_regs(slot->addr, APP_MODBUS_FC_HOLDING, m_regs, ARRAY_SIZE(m_regs),
				   &values, true);

	app_modbus_release();

	if (ret) {
		data->error_count++;
		data->valid = false;
		return ret;
	}

	values.valid = true;
	values.last_sample = k_uptime_get_32();
	values.error_count = 0;
	*data = values;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct or_we_504_sample rec = {
		.timestamp = ts,
		.voltage = data->voltage,
		.current = data->current,
		.frequency = data->frequency,
		.power = data->power,
		.power_reactive = data->power_reactive,
		.power_apparent = data->power_apparent,
		.power_factor = data->power_factor,
		.energy_in = data->energy_in,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("OR-WE-504: V=%.1f V, I=%.1f A, F=%.1f Hz, P=%.0f W, S=%.0f VA, Q=%.0f var, "
		"PF=%.3f, E=%.0f Wh",
		(double)data->voltage, (double)data->current, (double)data->frequency,
		(double)data->power, (double)data->power_apparent, (double)data->power_reactive,
		(double)data->power_factor, (double)data->energy_in);

	return 0;
}

static int config(struct app_driver_slot *slot, uint8_t new_addr, const char *parity_str)
{
	int ret;

	/* The meter has no parity setting (8N1, it also works on an 8E1 bus) */
	ARG_UNUSED(parity_str);

	if (new_addr < 1 || new_addr > 247) {
		LOG_ERR("Invalid Modbus address: %d (must be 1-247)", new_addr);
		return -EINVAL;
	}

	LOG_INF("Changing OR-WE-504 address %u -> %u", slot->addr, new_addr);

	ret = app_modbus_change_addr_or_we_504(slot->addr, new_addr);
	if (ret) {
		LOG_ERR("Failed to change the address: %d", ret);
		return ret;
	}

	return 0;
}

/* Shell commands */

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device or_we_504 sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_device_probe_sample(shell, &or_we_504_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	long addr = strtol(argv[1], NULL, 0);
	long new_addr = strtol(argv[2], NULL, 0);
	int ret;

	if (addr < 1 || addr > 247 || new_addr < 1 || new_addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	ret = app_modbus_shell_change_addr_or_we_504(shell, (uint8_t)addr, (uint8_t)new_addr);
	if (ret) {
		return ret;
	}

	shell_print(shell, "Then set the address in app config device-N");

	return 0;
}

/* Shell command entries */
static const struct shell_static_entry or_we_504_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Change address: config <addr> <new_addr>", cmd_config, 3, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry or_we_504_shell_cmds = {
	.entry = or_we_504_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_or_we_504 *data = slot->ctx;

	app_device_print_header(shell, slot, "OR-WE-504");
	shell_print(shell, "  voltage:        %.1f V", (double)data->voltage);
	shell_print(shell, "  current:        %.3f A", (double)data->current);
	shell_print(shell, "  frequency:      %.2f Hz", (double)data->frequency);
	shell_print(shell, "  power:          %.4f kW", (double)data->power);
	shell_print(shell, "  power_apparent: %.4f kVA", (double)data->power_apparent);
	shell_print(shell, "  power_reactive: %.4f kvar", (double)data->power_reactive);
	shell_print(shell, "  power_factor:   %.3f", (double)data->power_factor);
	shell_print(shell, "  energy_in:      %.3f kWh", (double)data->energy_in);
}

/* Driver registration */
const struct app_device_driver or_we_504_driver = {
	.name = "or_we_504",
	.type = APP_DEVICE_TYPE_OR_WE_504,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_or_we_504),
	.rec_size = sizeof(struct or_we_504_sample),
	.init = init,
	.sample = sample,
	.config = config,
	.print_data = print_data,
};

/* Public API */

const struct app_data_or_we_504 *or_we_504_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
