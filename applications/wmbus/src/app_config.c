/*
 * Copyright (c) 2024 HARDWARIO a.s.
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
#include <ctype.h>
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-wmbus"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("scan-timeout", m_config_interim.scan_timeout, 1, 86400, "Get/Set scan timeout.", 130),
	CTR_CONFIG_ITEM_INT("scan-interval", m_config_interim.scan_interval, 30, 604800, "Get/Set scan interval in seconds.", 600),
	CTR_CONFIG_ITEM_INT("scan-hour", m_config_interim.scan_hour, 0, 23, "Get/Set scan hour.", 12),
	CTR_CONFIG_ITEM_INT("scan-weekday", m_config_interim.scan_weekday, 0, 6, "Get/Set scan weekday.", 3),
	CTR_CONFIG_ITEM_INT("scan-day", m_config_interim.scan_day, 1, 31, "Get/Set scan day in the month.", 15),
	CTR_CONFIG_ITEM_ENUM("scan-mode", m_config_interim.scan_mode, ((const char*[]){"off", "interval", "daily", "weekly", "monthly"}), "Get/Set scan mode", 0),
	
	CTR_CONFIG_ITEM_ENUM("scan-ant", m_config_interim.scan_ant, ((const char*[]){"single", "dual"}), "Get/Set scan antenna", 0),
	
	CTR_CONFIG_ITEM_INT("poll-interval", m_config_interim.poll_interval, 5, 1209600, "Get/Set poll interval in seconds.", 28800),
	CTR_CONFIG_ITEM_INT("downlink-wdg-interval", m_config_interim.downlink_wdg_interval, 0, 1209600, "Get/Set poll interval in seconds.", 172800),
	CTR_CONFIG_ITEM_BOOL("cloud-decode", m_config_interim.cloud_decode, "Get/Set cloud decode.", false),

	CTR_CONFIG_ITEM_ENUM("mode", m_config_interim.mode, ((const char*[]){"none", "lte"}), "Set communication mode", APP_CONFIG_MODE_LTE),

	/* ### Preserved code "config" (begin) */
	/* ^^^ Preserved code "config" (end) */

};
/* clang-format on */

/* ### Preserved code "function" (begin) */
static void print_address_count(const struct shell *shell)
{
	size_t count;
	wmbus_get_config_device_count(&count);

	shell_print(shell, "app config address count %d", count);
}

static void print_address(const struct shell *shell)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_interim.address); i++) {
		if (m_config_interim.address[i] != 0) {
			shell_print(shell, "app config address add %d",
				    m_config_interim.address[i]);
		}
	}
}

int app_config_cmd_config_address(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_address(shell);
		return 0;
	}

	if (argc == 3 && strcmp(argv[1], "add") == 0) {
		size_t len = strlen(argv[2]);

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[2][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int address = atoi(argv[2]);

		if (address < 0 || address > UINT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		/* Check if address already exists */
		for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
			if (m_config_interim.address[i] == address) {
				shell_error(shell, "address already exists");
				return -EINVAL;
			}
		}

		/* Find next empty */
		int next_empty = -1;
		for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
			if (m_config_interim.address[i] == 0) {
				next_empty = i;
				break;
			}
		}

		if (next_empty == -1) {
			shell_error(shell, "no empty position");
			return -EINVAL;
		}

		m_config_interim.address[next_empty] = address;

		return 0;
	}

	if (argc == 3 && strcmp(argv[1], "remove") == 0) {
		size_t len = strlen(argv[2]);

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[2][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int address = atoi(argv[2]);

		if (address < 0 || address > UINT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int was_found = false;
		for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
			if (m_config_interim.address[i] == address) {
				was_found = true;
				m_config_interim.address[i] = 0;
			}
		}

		if (!was_found) {
			shell_error(shell, "item not found");
			return -EINVAL;
		}

		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "erase") == 0) {

		for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
			m_config_interim.address[i] = 0;
		}

		return 0;
	}

	/* This is pseudo-parameter, it is used for downlink config and doesn't matter which number
	 * is passed, it always erase all addresses and expects that next commands will call address
	 * add to fill them again */
	if (strcmp(argv[1], "count") == 0) {
		LOG_WRN("count");

		for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
			m_config_interim.address[i] = 0;
		}

		return 0;
	}

	/* DEBUG: Generate devices in config */
	if (argc == 3 && strcmp(argv[1], "gen") == 0) {
		size_t count = atoi(argv[2]);

		for (int i = 0; i < count; i++) {
			m_config_interim.address[i] = 81763000 + i;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}
/* ^^^ Preserved code "function" (end) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	/* ### Preserved code "app_config_cmd_config_show start" (begin) */
	print_address_count(shell);
	print_address(shell);
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
	if (argc > 1 && strcmp(argv[1], "address") == 0) {
		return app_config_cmd_config_address(shell, argc - 1, argv + 1);
	}

	if (argc > 1 && strcmp(argv[1], "show") == 0) {
		print_address_count(shell);
		print_address(shell);
	}
	/* ^^^ Preserved code "app_config_cmd_config" (end) */
	return ctr_config_cmd_config(items, ARRAY_SIZE(items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	/* ### Preserved code "h_set" (begin) */
	int ret;
	const char *next;
#define SETTINGS_SET_ARRAY(_key, _var, _size)                                                      \
	do {                                                                                       \
		if (settings_name_steq(key, #_key, &next) && !next) {                              \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, _var, len);                                          \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET_ARRAY("address", &m_config_interim.address, sizeof(m_config_interim.address));

#undef SETTINGS_SET_ARRAY
	/* ^^^ Preserved code "h_set" (end) */

	return ctr_config_h_set(items, ARRAY_SIZE(items), key, len, read_cb, cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	/* ### Preserved code "h_commit" (begin) */
	/* ^^^ Preserved code "h_commit" (end) */

	memcpy(&g_app_config, &m_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
/* ### Preserved code "h_export" (begin) */
#define EXPORT_FUNC_ARRAY(_key, _var, _size)                                                       \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" #_key, _var, _size);                            \
	} while (0)

	EXPORT_FUNC_ARRAY("address", &m_config_interim.address, sizeof(m_config_interim.address));

#undef EXPORT_FUNC_ARRAY
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
