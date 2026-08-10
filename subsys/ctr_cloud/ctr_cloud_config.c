/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_cloud_config, CONFIG_CTR_CLOUD_LOG_LEVEL);

#define SETTINGS_PFX "cloud"

struct ctr_cloud_config g_ctr_cloud_config;
static struct ctr_cloud_config m_config_interim;

static const struct ctr_config_item m_config_items[] = {
#if defined(CONFIG_CTR_CLOUD_SPOOL)
	CTR_CONFIG_ITEM_INT("spool-size", m_config_interim.spool_size, 0, 100,
			    "maximum number of messages stored in the spool", 10),
#endif
};

int ctr_cloud_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

int ctr_cloud_config_cmd(const struct shell *shell, size_t argc, char **argv)
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

	memcpy(&g_ctr_cloud_config, &m_config_interim, sizeof(struct ctr_cloud_config));

	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	return ctr_config_h_export(m_config_items, ARRAY_SIZE(m_config_items), export_func);
}

static int init(void)
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

	ctr_config_append_show(SETTINGS_PFX, ctr_cloud_config_cmd_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
