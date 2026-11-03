/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_power_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(ctr_power_config, CONFIG_CTR_POWER_LOG_LEVEL);

#define SETTINGS_PFX "power"

struct ctr_power_config g_ctr_power_config;
static struct ctr_power_config m_config_interim;

static const char *m_enum_source_items[] = {
	[CTR_POWER_CONFIG_SOURCE_AUTO] = "auto",
	[CTR_POWER_CONFIG_SOURCE_NONE] = "none",
	[CTR_POWER_CONFIG_SOURCE_CTR_Z] = "ctr-z",
	[CTR_POWER_CONFIG_SOURCE_CTR_X4_A] = "ctr-x4-a",
	[CTR_POWER_CONFIG_SOURCE_CTR_X4_B] = "ctr-x4-b",
	[CTR_POWER_CONFIG_SOURCE_CTR_X10] = "ctr-x10",
};

/* clang-format off */
static const struct ctr_config_item m_config_items[] = {
	CTR_CONFIG_ITEM_ENUM("source", m_config_interim.source, m_enum_source_items, "Get/Set external power source.", CTR_POWER_CONFIG_SOURCE_AUTO),
	CTR_CONFIG_ITEM_INT("interval-sample", m_config_interim.interval_sample, 1, 86400, "Get/Set sample interval in seconds.", 300),
	CTR_CONFIG_ITEM_BOOL("report-connected", m_config_interim.report_connected, "Get/Set report when line is connected.", true),
	CTR_CONFIG_ITEM_BOOL("report-disconnected", m_config_interim.report_disconnected, "Get/Set report when line is disconnected.", true),
};
/* clang-format on */

int ctr_power_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

int ctr_power_config_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return ctr_config_cmd_config(m_config_items, ARRAY_SIZE(m_config_items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	return ctr_config_h_set(m_config_items, ARRAY_SIZE(m_config_items), key, len, read_cb,
				cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_ctr_power_config, &m_config_interim, sizeof(struct ctr_power_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	return ctr_config_h_export(m_config_items, ARRAY_SIZE(m_config_items), export_func);
}

int ctr_power_config_init(void)
{
	int ret;

	LOG_INF("System initialization");

	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_init_item(&m_config_items[i]);
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

	ctr_config_append_show(SETTINGS_PFX, ctr_power_config_cmd_show);

	return 0;
}

static int init(void)
{
	return ctr_power_config_init();
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_POWER_INIT_PRIORITY);
