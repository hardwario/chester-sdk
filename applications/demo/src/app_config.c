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
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-demo"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("interval-sample", m_config_interim.interval_sample, 1, 86400, "Get/Set sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("interval-aggreg", m_config_interim.interval_aggreg, 1, 86400, "Get/Set aggregate interval in seconds .", 300),
	CTR_CONFIG_ITEM_INT("interval-report", m_config_interim.interval_report, 30, 86400, "Get/Set report interval in seconds.", 1800),
	CTR_CONFIG_ITEM_INT("interval-poll", m_config_interim.interval_poll, 0, 86400, "Get/Set poll interval in seconds (disabled if 0).", 60),

	CTR_CONFIG_ITEM_ENUM("mode", m_config_interim.mode, ((const char*[]){"none", "lte"}), "Set communication mode", APP_CONFIG_MODE_LTE),

	/* ### Preserved code "config" (begin) */
	/* ^^^ Preserved code "config" (end) */

};
/* clang-format on */

/* ### Preserved code "function" (begin) */
/* ^^^ Preserved code "function" (end) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(items); i++) {
		ctr_config_show_item(shell, &items[i]);
	}

	return 0;
}

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	return ctr_config_cmd_config(items, ARRAY_SIZE(items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	return ctr_config_h_set(items, ARRAY_SIZE(items), key, len, read_cb, cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_app_config, &m_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
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
