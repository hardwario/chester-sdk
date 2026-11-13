/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_cubic_6303.c
 * @brief Cubic PM6303 / OPM6303 particulate matter sensor driver
 *
 * Table-driven: input registers (FC04), particulate concentrations as unsigned 32-bit
 * values with the high word first (ug/m3), the gas flow as an unsigned 16-bit value
 * scaled by 100. All items are aggregated to min / max / avg / median per interval-aggreg
 * window. The firmware version register (0) is not a measurement and is not read.
 *
 * The sensor address is changed by writing holding register 2. The sensor answers that
 * write from the new address, so the change is checked by reading the register back
 * (app_modbus_change_addr).
 */

#include "drv_interface.h"
#include "../app_modbus.h"

/* Generated codec keys */
#include "app_codec.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(drv_cubic_6303, LOG_LEVEL_DBG);

#define REG_ADDRESS 2 /* Holding register, sensor Modbus address */

static const struct app_driver_item m_items[] = {
	APP_DRIVER_ITEM("tsp", "µg/m³", 5, APP_MODBUS_U32_HI, 1.f, APP_DRIVER_AGGREG_STATS, 1.f,
			CODEC_KEY_E_DEVICES__CUBIC_6303__TSP),
	APP_DRIVER_ITEM("pm1_0", "µg/m³", 7, APP_MODBUS_U32_HI, 1.f, APP_DRIVER_AGGREG_STATS, 1.f,
			CODEC_KEY_E_DEVICES__CUBIC_6303__PM1_0),
	APP_DRIVER_ITEM("pm2_5", "µg/m³", 9, APP_MODBUS_U32_HI, 1.f, APP_DRIVER_AGGREG_STATS, 1.f,
			CODEC_KEY_E_DEVICES__CUBIC_6303__PM2_5),
	APP_DRIVER_ITEM("pm10", "µg/m³", 13, APP_MODBUS_U32_HI, 1.f, APP_DRIVER_AGGREG_STATS, 1.f,
			CODEC_KEY_E_DEVICES__CUBIC_6303__PM10),
	APP_DRIVER_ITEM("gas_flow", "", 23, APP_MODBUS_U16, 100.f, APP_DRIVER_AGGREG_STATS, 100.f,
			CODEC_KEY_E_DEVICES__CUBIC_6303__GAS_FLOW),
};

BUILD_ASSERT(ARRAY_SIZE(m_items) <= APP_DRIVER_MAX_ITEMS);

static int config(struct app_driver_slot *slot, uint8_t new_addr, const char *parity_str)
{
	enum app_modbus_addr_result result;
	int ret;

	ARG_UNUSED(parity_str);

	LOG_INF("Changing Cubic 6303 address %u -> %u", slot->addr, new_addr);

	ret = app_modbus_change_addr(slot->addr, new_addr, REG_ADDRESS, &result);
	if (ret) {
		LOG_ERR("Failed to change the address: %d", ret);
		return ret;
	}

	if (result == APP_MODBUS_ADDR_PENDING) {
		LOG_WRN("Power-cycle the sensor to apply the new address");
	}

	return 0;
}

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	app_device_print_items(shell, slot, "Cubic 6303");
}

const struct app_device_driver cubic_6303_driver = {
	.name = "cubic_6303",
	.type = APP_DEVICE_TYPE_CUBIC_6303,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.items = m_items,
	.item_count = ARRAY_SIZE(m_items),
	.fc = APP_MODBUS_FC_INPUT,
	.codec_key = CODEC_KEY_E_DEVICES__CUBIC_6303,
	.config = config,
	.print_data = print_data,
};

/* Shell commands */

static int parse_addr(const struct shell *shell, const char *arg, uint8_t *addr)
{
	long val = strtol(arg, NULL, 0);

	if (val < 1 || val > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	*addr = (uint8_t)val;
	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t addr;
	int ret;

	ret = parse_addr(shell, argv[1], &addr);
	if (ret) {
		return ret;
	}

	ret = app_device_probe_sample(shell, &cubic_6303_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t addr;
	uint8_t new_addr;
	int ret;

	if (parse_addr(shell, argv[1], &addr) || parse_addr(shell, argv[2], &new_addr)) {
		return -EINVAL;
	}

	ret = app_modbus_shell_change_addr(shell, addr, new_addr, REG_ADDRESS);
	if (ret) {
		return ret;
	}

	shell_print(shell, "Then set the address in app config device-N");

	return 0;
}

static const struct shell_static_entry cubic_6303_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Change address: config <addr> <new_addr>", cmd_config, 3, 0),
	SHELL_SUBCMD_SET_END,
};

const union shell_cmd_entry cubic_6303_shell_cmds = {
	.entry = cubic_6303_subcmds,
};
