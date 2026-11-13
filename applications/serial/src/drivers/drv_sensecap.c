/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_sensecap.c
 * @brief Seeed SenseCAP S1000 / S700 / S500 compact weather station driver
 *
 * Table-driven: every item is an input register (FC04) pair holding a signed 32-bit value
 * with the high word first, scaled by 1000. The S500 measures the first five items of the
 * S1000 table, the S700 the first seven plus the heater temperature and the tilt state.
 * Items are aggregated per interval-aggreg window: wind direction as a vector mean, the
 * accumulated rainfall as a counter (last absolute value), the S700 diagnostics as the last
 * value, the rest as min / max / avg / median.
 *
 * The station address is changed by writing holding register 0x1000; the station confirms
 * it from the old address and applies it after a power cycle (app_modbus_change_addr reads
 * the register back when the reply goes astray).
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
#include <string.h>

LOG_MODULE_REGISTER(drv_sensecap, LOG_LEVEL_DBG);

#define REG_ADDRESS 0x1000 /* Holding register, station Modbus address */

/* The S500 measures the first five items of the S1000 table */
#define S500_ITEM_COUNT 5

/*
 * Items of the S1000 and the S700 table; the first five are the S500 items. The LoRaWAN
 * encoding is positional, so the order must not change.
 */
/* clang-format off */
#define COMMON_ITEMS                                                                               \
	APP_DRIVER_ITEM("air_temperature", "°C", 0x0000, APP_MODBUS_S32_HI, 1000.f,                \
			APP_DRIVER_AGGREG_STATS, 1000.f,                                           \
			CODEC_KEY_E_DEVICES__SENSECAP__AIR_TEMPERATURE),                           \
	APP_DRIVER_ITEM("air_humidity", "%", 0x0002, APP_MODBUS_S32_HI, 1000.f,                    \
			APP_DRIVER_AGGREG_STATS, 1000.f,                                           \
			CODEC_KEY_E_DEVICES__SENSECAP__AIR_HUMIDITY),                              \
	/* LoRaWAN: Float16 cannot hold Pa (max 65504), send 0.1 hPa as U16 */                     \
	APP_DRIVER_ITEM_LRW("barometric_pressure", "Pa", 0x0004, APP_MODBUS_S32_HI, 1000.f,        \
			    APP_DRIVER_AGGREG_STATS, 1000.f,                                       \
			    CODEC_KEY_E_DEVICES__SENSECAP__BAROMETRIC_PRESSURE, APP_DRIVER_LRW_U16, \
			    0.1f),                                                                 \
	/* Station-side average direction; a min / max / mean of an angle is meaningless */        \
	APP_DRIVER_ITEM("wind_direction_avg", "°", 0x000C, APP_MODBUS_S32_HI, 1000.f,              \
			APP_DRIVER_AGGREG_CIRCULAR, 1000.f,                                        \
			CODEC_KEY_E_DEVICES__SENSECAP__WIND_DIRECTION_AVG),                        \
	APP_DRIVER_ITEM("wind_speed_avg", "m/s", 0x0012, APP_MODBUS_S32_HI, 1000.f,                \
			APP_DRIVER_AGGREG_STATS, 1000.f,                                           \
			CODEC_KEY_E_DEVICES__SENSECAP__WIND_SPEED_AVG),                            \
	/* LoRaWAN: sunlight exceeds the Float16 range in lux, send klux */                        \
	APP_DRIVER_ITEM_LRW("light_intensity", "lux", 0x0006, APP_MODBUS_S32_HI, 1000.f,           \
			    APP_DRIVER_AGGREG_STATS, 1000.f,                                       \
			    CODEC_KEY_E_DEVICES__SENSECAP__LIGHT_INTENSITY, APP_DRIVER_LRW_F16,    \
			    0.001f),                                                               \
	/* Running total: the cloud computes differences, so a lost message loses nothing */       \
	APP_DRIVER_ITEM("accumulated_rainfall", "mm", 0x0014, APP_MODBUS_S32_HI, 1000.f,           \
			APP_DRIVER_AGGREG_COUNTER, 1000.f,                                         \
			CODEC_KEY_E_DEVICES__SENSECAP__ACCUMULATED_RAINFALL)
/* clang-format on */

static const struct app_driver_item m_items[] = {
	COMMON_ITEMS,
	/* S1000 only */
	APP_DRIVER_ITEM("pm2_5", "µg/m³", 0x0030, APP_MODBUS_S32_HI, 1000.f,
			APP_DRIVER_AGGREG_STATS, 1000.f, CODEC_KEY_E_DEVICES__SENSECAP__PM2_5),
	APP_DRIVER_ITEM("pm10", "µg/m³", 0x0032, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_STATS,
			1000.f, CODEC_KEY_E_DEVICES__SENSECAP__PM10),
	APP_DRIVER_ITEM("co2", "ppm", 0x0040, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_STATS,
			1000.f, CODEC_KEY_E_DEVICES__SENSECAP__CO2),
};

static const struct app_driver_item m_items_s700[] = {
	COMMON_ITEMS,
	/* S700 diagnostics, the last value of each window */
	APP_DRIVER_ITEM("heater_temperature", "°C", 0x001C, APP_MODBUS_S32_HI, 1000.f,
			APP_DRIVER_AGGREG_LAST, 1000.f,
			CODEC_KEY_E_DEVICES__SENSECAP__HEATER_TEMPERATURE),
	/* 0 upright, 1 tipped over */
	APP_DRIVER_ITEM("tilt", "", 0x001E, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_LAST,
			1000.f, CODEC_KEY_E_DEVICES__SENSECAP__TILT),
};

BUILD_ASSERT(ARRAY_SIZE(m_items) <= APP_DRIVER_MAX_ITEMS);
BUILD_ASSERT(ARRAY_SIZE(m_items_s700) <= APP_DRIVER_MAX_ITEMS);

static int config(struct app_driver_slot *slot, uint8_t new_addr, const char *parity_str)
{
	enum app_modbus_addr_result result;
	int ret;

	ARG_UNUSED(parity_str);

	LOG_INF("Changing SenseCAP address %u -> %u", slot->addr, new_addr);

	ret = app_modbus_change_addr(slot->addr, new_addr, REG_ADDRESS, &result);
	if (ret) {
		LOG_ERR("Failed to change the address: %d", ret);
		return ret;
	}

	if (result == APP_MODBUS_ADDR_PENDING) {
		LOG_WRN("Power-cycle the station to apply the new address");
	}

	return 0;
}

static void print_s1000(const struct shell *shell, struct app_driver_slot *slot)
{
	app_device_print_items(shell, slot, "SenseCAP S1000");
}

static void print_s500(const struct shell *shell, struct app_driver_slot *slot)
{
	app_device_print_items(shell, slot, "SenseCAP S500");
}

static void print_s700(const struct shell *shell, struct app_driver_slot *slot)
{
	app_device_print_items(shell, slot, "SenseCAP S700");
}

const struct app_device_driver sensecap_s1000_driver = {
	.name = "sensecap_s1000",
	.type = APP_DEVICE_TYPE_SENSECAP_S1000,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.items = m_items,
	.item_count = ARRAY_SIZE(m_items),
	.fc = APP_MODBUS_FC_INPUT,
	.codec_key = CODEC_KEY_E_DEVICES__SENSECAP,
	.config = config,
	.print_data = print_s1000,
};

const struct app_device_driver sensecap_s500_driver = {
	.name = "sensecap_s500",
	.type = APP_DEVICE_TYPE_SENSECAP_S500,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.items = m_items,
	.item_count = S500_ITEM_COUNT,
	.fc = APP_MODBUS_FC_INPUT,
	.codec_key = CODEC_KEY_E_DEVICES__SENSECAP,
	.config = config,
	.print_data = print_s500,
};

const struct app_device_driver sensecap_s700_driver = {
	.name = "sensecap_s700",
	.type = APP_DEVICE_TYPE_SENSECAP_S700,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.items = m_items_s700,
	.item_count = ARRAY_SIZE(m_items_s700),
	.fc = APP_MODBUS_FC_INPUT,
	.codec_key = CODEC_KEY_E_DEVICES__SENSECAP,
	.config = config,
	.print_data = print_s700,
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
	const struct app_device_driver *drv = &sensecap_s1000_driver;
	uint8_t addr;
	int ret;

	ret = parse_addr(shell, argv[1], &addr);
	if (ret) {
		return ret;
	}

	/* The S1000 table by default; on an S500 or S700 its S1000-only reads fail */
	if (argc > 2) {
		if (strcmp(argv[2], "s500") == 0) {
			drv = &sensecap_s500_driver;
		} else if (strcmp(argv[2], "s700") == 0) {
			drv = &sensecap_s700_driver;
		} else if (strcmp(argv[2], "s1000") != 0) {
			shell_error(shell, "Unknown model: %s (s1000, s700 or s500)", argv[2]);
			return -EINVAL;
		}
	}

	ret = app_device_probe_sample(shell, drv, addr);
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

static const struct shell_static_entry sensecap_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr> [s1000|s700|s500]", cmd_sample, 2,
		      1),
	SHELL_CMD_ARG(config, NULL, "Change address: config <addr> <new_addr>", cmd_config, 3, 0),
	SHELL_SUBCMD_SET_END,
};

const union shell_cmd_entry sensecap_shell_cmds = {
	.entry = sensecap_subcmds,
};
