/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_tok.h"

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

static int bands_parse_cb(const struct shell *shell, char *argv,
			  const struct ctr_config_item *item);

static struct ctr_config_item m_config_items[] = {
	CTR_CONFIG_ITEM_BOOL("test", m_config_interim.test, "LTE V2 test", false),
	CTR_CONFIG_ITEM_ENUM("antenna", m_config_interim.antenna, m_enum_antenna_items,
			     "antenna type", CONFIG_CTR_LTE_V2_CONFIG_ANTENNA),
	CTR_CONFIG_ITEM_BOOL("nb-iot-mode", m_config_interim.nb_iot_mode, "NB-IoT mode",
			     IS_ENABLED(CONFIG_CTR_LTE_V2_DEFAULT_NB_IOT_MODE)),
	CTR_CONFIG_ITEM_BOOL("lte-m-mode", m_config_interim.lte_m_mode, "LTE-M mode",
			     IS_ENABLED(CONFIG_CTR_LTE_V2_DEFAULT_LTE_M_MODE)),
	CTR_CONFIG_ITEM_STRING_PARSE_CB(
		"bands", m_config_interim.bands,
		"supported bands (\"auto\" or listed with comma separator): \n"
		"                     - LTE-M:  "
		"1,2,3,4,5,8,12,13,18,19,20,25,26,28,66\n"
		"                     - NB-IoT: "
		"1,2,3,4,5,8,12,13,17,19,20,25,26,28,66",
		CONFIG_CTR_LTE_V2_DEFAULT_BANDS, bands_parse_cb),
	CTR_CONFIG_ITEM_BOOL("autoconn", m_config_interim.autoconn, "auto-connect feature", false),
	CTR_CONFIG_ITEM_STRING("plmnid", m_config_interim.plmnid,
			       "network PLMN ID (format: 5-6 digits)", "23003"),
	CTR_CONFIG_ITEM_STRING("apn", m_config_interim.apn, "network APN",
			       CONFIG_CTR_LTE_V2_DEFAULT_APN),
	CTR_CONFIG_ITEM_ENUM("auth", m_config_interim.auth, m_enum_auth_items,
			     "authentication protocol", CTR_LTE_V2_CONFIG_AUTH_NONE),
	CTR_CONFIG_ITEM_STRING("username", m_config_interim.username, "username", ""),
	CTR_CONFIG_ITEM_STRING("password", m_config_interim.password, "password", ""),
	CTR_CONFIG_ITEM_STRING("addr", m_config_interim.addr, "default IP address",
			       "192.168.192.4"),
	CTR_CONFIG_ITEM_INT("port", m_config_interim.port, 1, 65536, "default UDP port", 5002),
	CTR_CONFIG_ITEM_BOOL("modemtrace", m_config_interim.modemtrace, "modemtrace", false),
};

static bool is_supported_band(uint8_t band)
{
	static const uint8_t support_bands[] = {1,  2,  3,  4,  5,  8,  12, 13,
						17, 18, 19, 20, 25, 26, 28, 66};
	for (size_t i = 0; i < ARRAY_SIZE(support_bands); i++) {
		if (band == support_bands[i]) {
			return true;
		}
	}
	return false;
}

int bands_parse_cb(const struct shell *shell, char *argv, const struct ctr_config_item *item)
{
	size_t len = strlen(argv);
	if (len >= item->size) {
		shell_error(shell, "value too long");
		ctr_config_help_item(shell, item);
		return -ENOMEM;
	}

	if (strncmp(argv, "auto", 4) == 0) {
		strncpy(item->variable, argv, item->size - 1);
		((char *)item->variable)[item->size - 1] = '\0';
		return 0;
	}

	const char *p = argv;

	bool def;
	long band;

	while (p) {
		if (!(p = ctr_lte_v2_tok_num(p, &def, &band)) || !def || band < 0 || band > 255) {
			shell_error(shell, "invalid number format");
			return -EINVAL;
		}

		if (!is_supported_band((uint8_t)band)) {
			shell_error(shell, "band %d is not supported", (uint8_t)band);
			return -EINVAL;
		}

		if (ctr_lte_v2_tok_end(p)) {
			break;
		}

		if (!(p = ctr_lte_v2_tok_sep(p))) {
			shell_error(shell, "expected comma after %d", (uint8_t)band);
			return -EINVAL;
		}
	}

	strncpy(item->variable, argv, item->size - 1);
	((char *)item->variable)[item->size - 1] = '\0';

	return 0;
}

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
