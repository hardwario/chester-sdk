/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2_config, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define SETTINGS_PFX "lte"

struct ctr_lte_v2_config g_ctr_lte_v2_config;
static struct ctr_lte_v2_config m_config_interim;

static const char *m_enum_antenna_items[] = {"int", "ext"};
static const char *m_enum_auth_items[] = {"none", "pap", "chap"};

static struct ctr_config_item m_config_items[] = {
	CTR_CONFIG_ITEM_BOOL("test", m_config_interim.test, "LTE V2 test", false),
	CTR_CONFIG_ITEM_ENUM("antenna", m_config_interim.antenna, m_enum_antenna_items,
			     "antenna type", CTR_LTE_V2_CONFIG_ANTENNA_INT),
	CTR_CONFIG_ITEM_BOOL("nb-iot-mode", m_config_interim.nb_iot_mode, "NB-IoT mode", true),
	CTR_CONFIG_ITEM_BOOL("lte-m-mode", m_config_interim.lte_m_mode, "LTE-M mode", false),
	CTR_CONFIG_ITEM_BOOL("autoconn", m_config_interim.autoconn, "auto-connect feature", false),
	CTR_CONFIG_ITEM_STRING("plmnid", m_config_interim.plmnid,
			       "network PLMN ID (format: 5-6 digits)", "23003"),
	CTR_CONFIG_ITEM_STRING("apn", m_config_interim.apn, "network APN", "hardwario"),
	CTR_CONFIG_ITEM_ENUM("auth", m_config_interim.auth, m_enum_auth_items,
			     "authentication protocol", CTR_LTE_V2_CONFIG_AUTH_NONE),
	CTR_CONFIG_ITEM_STRING("username", m_config_interim.username, "username", ""),
	CTR_CONFIG_ITEM_STRING("password", m_config_interim.password, "password", ""),
	CTR_CONFIG_ITEM_STRING("addr", m_config_interim.addr, "default IP address",
			       "192.168.192.4"),
	CTR_CONFIG_ITEM_INT("port", m_config_interim.port, 1, 65536, "default UDP port", 5002),
	CTR_CONFIG_ITEM_BOOL("modemtrace", m_config_interim.modemtrace, "modemtrace", false),
};

int ctr_lte_v2_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

int ctr_lte_v2_config_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return ctr_config_cmd_config(m_config_items, ARRAY_SIZE(m_config_items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strncmp(key, "clksync", 7) == 0) { /* obsolete key, ignore load */
		return 0;
	}
	return ctr_config_h_set(m_config_items, ARRAY_SIZE(m_config_items), key, len, read_cb,
				cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_ctr_lte_v2_config, &m_config_interim, sizeof(struct ctr_lte_v2_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	return ctr_config_h_export(m_config_items, ARRAY_SIZE(m_config_items), export_func);
}

int ctr_lte_v2_config_init(void)
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

	ctr_config_append_show(SETTINGS_PFX, ctr_lte_v2_config_cmd_show);

	return 0;
}
