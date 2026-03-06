/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ### Preserved code "includes" (begin) */
#include "feature.h"

/* Forward declaration for device_parse_cb used in items[] */
static int device_parse_cb(const struct shell *shell, char *argv,
			   const struct ctr_config_item *item);
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-serial"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("interval-sample", m_config_interim.interval_sample, 1, 86400, "Sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("interval-aggreg", m_config_interim.interval_aggreg, 1, 86400, "Aggregation interval in seconds.", 300),
	CTR_CONFIG_ITEM_INT("interval-report", m_config_interim.interval_report, 0, 86400, "Report interval in seconds.", 1800),
	CTR_CONFIG_ITEM_INT("interval-poll", m_config_interim.interval_poll, 0, 86400, "Polling interval in seconds (LTE only, 0 = disabled).", 0),
	CTR_CONFIG_ITEM_ENUM("serial-mode", m_config_interim.serial_mode, ((const char*[]){"transparent", "modbus"}), "Serial operation mode", 0),
	
	CTR_CONFIG_ITEM_INT("serial-baudrate", m_config_interim.serial_baudrate, 1200, 115200, "Serial baudrate.", 9600),
	CTR_CONFIG_ITEM_INT("serial-data-bits", m_config_interim.serial_data_bits, 7, 9, "Serial data bits.", 8),
	CTR_CONFIG_ITEM_ENUM("serial-parity", m_config_interim.serial_parity, ((const char*[]){"none", "odd", "even"}), "Serial parity", 0),
	
	CTR_CONFIG_ITEM_INT("serial-stop-bits", m_config_interim.serial_stop_bits, 1, 2, "Serial stop bits.", 1),

	CTR_CONFIG_ITEM_ENUM("mode", m_config_interim.mode, ((const char*[]){"none", "lte", "lrw"}), "Set communication mode", APP_CONFIG_MODE_NONE),

	/* ### Preserved code "config" (begin) */
	/* Device configuration - type[,addr[,timeout]] */
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-0", m_config_interim.device_str[0],
		"Device 0: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-1", m_config_interim.device_str[1],
		"Device 1: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-2", m_config_interim.device_str[2],
		"Device 2: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-3", m_config_interim.device_str[3],
		"Device 3: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-4", m_config_interim.device_str[4],
		"Device 4: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-5", m_config_interim.device_str[5],
		"Device 5: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-6", m_config_interim.device_str[6],
		"Device 6: type[,addr[,timeout]]", "", device_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("device-7", m_config_interim.device_str[7],
		"Device 7: type[,addr[,timeout]]", "", device_parse_cb),
	/* ^^^ Preserved code "config" (end) */

};
/* clang-format on */

/* ### Preserved code "function" (begin) */
/* Parse device config string: "type[,addr[,timeout]]" into struct */
static int parse_device_string(const char *input, struct app_config_device *dev)
{
	char buf[APP_CONFIG_DEVICE_STR_SIZE];
	char *token;
	char *saveptr;

	if (!dev) {
		return -EINVAL;
	}

	/* Default values */
	dev->type = APP_DEVICE_TYPE_NONE;
	dev->addr = 0;
	dev->timeout_s = 1;

	if (!input || !input[0]) {
		return 0;
	}

	strncpy(buf, input, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	/* Parse type (first parameter, required) */
	token = strtok_r(buf, ",", &saveptr);
	if (!token) {
		return 0;
	}

	enum app_device_type type = app_device_type_from_name(token);
	if (type == APP_DEVICE_TYPE_NONE && strcmp(token, "none") != 0) {
		return -EINVAL;
	}
	dev->type = type;

	/* Parse optional address */
	token = strtok_r(NULL, ",", &saveptr);
	if (token) {
		int addr = atoi(token);
		dev->addr = (addr > 0 && addr <= 247) ? addr : 0;

		/* Parse optional timeout */
		token = strtok_r(NULL, ",", &saveptr);
		if (token) {
			int timeout = atoi(token);
			dev->timeout_s = (timeout > 0) ? timeout : 1;
		}
	}

	return 0;
}

/* Device config parse callback - validates and stores string */
static int device_parse_cb(const struct shell *shell, char *argv,
			   const struct ctr_config_item *item)
{
	size_t len = strlen(argv);

	if (len >= item->size) {
		shell_error(shell, "Value too long (max %d)", item->size - 1);
		return -ENOMEM;
	}

	/* Empty string = none */
	if (len == 0) {
		memset(item->variable, 0, item->size);
		return 0;
	}

	/* Validate by attempting to parse */
	struct app_config_device test_dev;
	int ret = parse_device_string(argv, &test_dev);
	if (ret) {
		shell_error(shell, "Invalid format. Use: type[,addr[,timeout]]");
		shell_print(shell, "  Types: none, generic, cubic_6303, em1xx, em5xx,");
		shell_print(shell, "         flowt_ft201, iem3000, lambrecht, microsens_180hs,");
		shell_print(shell, "         or_we_504, or_we_516, promag_mf7s, sensecap_s1000");
		shell_print(shell, "  addr:  1-247 (required for Modbus mode)");
		shell_print(shell, "  timeout: seconds (default 1)");
		return ret;
	}

	strncpy(item->variable, argv, item->size - 1);
	((char *)item->variable)[item->size - 1] = '\0';

	return 0;
}

/* Validate device configuration before save */
static int validate_device_config(const struct shell *shell)
{
	/* Parse all device strings for validation */
	struct app_config_device devices[APP_CONFIG_MAX_DEVICES];
	int device_count = 0;

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		parse_device_string(m_config_interim.device_str[i], &devices[i]);
		if (devices[i].type != APP_DEVICE_TYPE_NONE) {
			device_count++;
		}
	}

#if defined(FEATURE_HARDWARE_CHESTER_X12_A)
	/* RS-232: only one device allowed (point-to-point) */
	if (device_count > 1) {
		shell_error(shell, "RS-232: max 1 device allowed (found %d)", device_count);
		return -EINVAL;
	}
#endif

	if (m_config_interim.serial_mode == APP_CONFIG_SERIAL_MODE_MODBUS) {
		/* Modbus mode: address required for all active devices */
		for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
			if (devices[i].type != APP_DEVICE_TYPE_NONE && devices[i].addr == 0) {
				shell_error(shell,
					    "Modbus mode: device-%d requires address (1-247)", i);
				return -EINVAL;
			}
		}
	}

	if (m_config_interim.serial_mode == APP_CONFIG_SERIAL_MODE_TRANSPARENT) {
		/* Transparent mode: only device-0 allowed */
		for (int i = 1; i < APP_CONFIG_MAX_DEVICES; i++) {
			if (devices[i].type != APP_DEVICE_TYPE_NONE) {
				shell_error(shell,
					    "Transparent mode: only device-0 allowed (device-%d "
					    "is set)",
					    i);
				return -EINVAL;
			}
		}
	}

	return 0;
}
/* ^^^ Preserved code "function" (end) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	/* ### Preserved code "app_config_cmd_config_show start" (begin) */
	/* ^^^ Preserved code "app_config_cmd_config_show start" (end) */

	for (int i = 0; i < ARRAY_SIZE(items); i++) {
		ctr_config_show_item(shell, &items[i]);
	}

	/* ### Preserved code "app_config_cmd_config_show end" (begin) */
	/* ^^^ Preserved code "app_config_cmd_config_show end" (end) */

	return 0;
}

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	/* ### Preserved code "app_config_cmd_config" (begin) */
	/* Intercept save command for validation */
	if (argc >= 2 && strcmp(argv[1], "save") == 0) {
		int ret = validate_device_config(shell);
		if (ret) {
			return ret;
		}
		/* Continue to ctr_config_cmd_config for actual save */
	}
	/* ^^^ Preserved code "app_config_cmd_config" (end) */
	return ctr_config_cmd_config(items, ARRAY_SIZE(items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	/* ### Preserved code "h_set" (begin) */
	/* ^^^ Preserved code "h_set" (end) */

	return ctr_config_h_set(items, ARRAY_SIZE(items), key, len, read_cb, cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	/* ### Preserved code "h_commit" (begin) */
	/* Parse device strings into struct devices array */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		parse_device_string(m_config_interim.device_str[i], &m_config_interim.devices[i]);
	}
	/* ^^^ Preserved code "h_commit" (end) */

	memcpy(&g_app_config, &m_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	/* ### Preserved code "h_export" (begin) */
	/* ^^^ Preserved code "h_export" (end) */

	return ctr_config_h_export(items, ARRAY_SIZE(items), export_func);
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	for (int i = 0; i < ARRAY_SIZE(items); i++) {
		ctr_config_init_item(&items[i]);
	}

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	/* ### Preserved code "init" (begin) */
	/* ^^^ Preserved code "init" (end) */

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
