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

#define SETTINGS_PFX "app-control"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("interval-report", m_config_interim.interval_report, 30, 86400, "Get/Set report interval in seconds.", 1800),
	CTR_CONFIG_ITEM_INT("interval-poll", m_config_interim.interval_poll, 0, 86400, "Get/Set poll interval in seconds.", 0),

#if defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X0_A)
	CTR_CONFIG_ITEM_INT("event-report-delay", m_config_interim.event_report_delay, 1, 86400, "Get/Set event report delay in seconds.", 5),
	CTR_CONFIG_ITEM_INT("event-report-rate", m_config_interim.event_report_rate, 1, 3600, "Get/Set event report rate in reports per hour.", 30),
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	CTR_CONFIG_ITEM_BOOL("backup-report-connected", m_config_interim.backup_report_connected, "Get/Set report when backup is active.", true),
	CTR_CONFIG_ITEM_BOOL("backup-report-disconnected", m_config_interim.backup_report_disconnected, "Get/Set report when backup is inactive.", true),
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	CTR_CONFIG_ITEM_ENUM("channel-mode-1", m_config_interim.channel_mode_1, ((const char*[]){"trigger", "counter", "voltage", "current"}), "Get/Set channel-mode-1", APP_CONFIG_CHANNEL_MODE_TRIGGER),
	CTR_CONFIG_ITEM_ENUM("channel-mode-2", m_config_interim.channel_mode_2, ((const char*[]){"trigger", "counter", "voltage", "current"}), "Get/Set channel-mode-2", APP_CONFIG_CHANNEL_MODE_COUNTER),
	CTR_CONFIG_ITEM_ENUM("channel-mode-3", m_config_interim.channel_mode_3, ((const char*[]){"trigger", "counter", "voltage", "current"}), "Get/Set channel-mode-3", APP_CONFIG_CHANNEL_MODE_VOLTAGE),
	CTR_CONFIG_ITEM_ENUM("channel-mode-4", m_config_interim.channel_mode_4, ((const char*[]){"trigger", "counter", "voltage", "current"}), "Get/Set channel-mode-4", APP_CONFIG_CHANNEL_MODE_CURRENT),
	CTR_CONFIG_ITEM_ENUM("trigger-input-type", m_config_interim.trigger_input_type, ((const char*[]){"npn", "pnp"}), "Get/Set trigger input type", APP_CONFIG_INPUT_TYPE_NPN),
	CTR_CONFIG_ITEM_ENUM("counter-input-type", m_config_interim.counter_input_type, ((const char*[]){"npn", "pnp"}), "Get/Set counter input type", APP_CONFIG_INPUT_TYPE_NPN),
	CTR_CONFIG_ITEM_INT("trigger-duration-active", m_config_interim.trigger_duration_active, 0, 60000, "Get/Set event active duration in milliseconds.", 100),
	CTR_CONFIG_ITEM_INT("trigger-duration-inactive", m_config_interim.trigger_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds.", 100),
	CTR_CONFIG_ITEM_INT("trigger-cooldown-time", m_config_interim.trigger_cooldown_time, 0, 60000, "Get/Set trigger cooldown time in milliseconds.", 10),
	CTR_CONFIG_ITEM_BOOL("trigger-report-active", m_config_interim.trigger_report_active, "Get/Set report when trigger is active.", false),
	CTR_CONFIG_ITEM_BOOL("trigger-report-inactive", m_config_interim.trigger_report_inactive, "Get/Set report when trigger is inactive.", false),
	CTR_CONFIG_ITEM_INT("counter-interval-aggreg", m_config_interim.counter_interval_aggreg, 1, 86400, "Get/Set counter aggregation interval in seconds.", 300),
	CTR_CONFIG_ITEM_INT("counter-duration-active", m_config_interim.counter_duration_active, 0, 60000, "Get/Set event active duration in milliseconds.", 2),
	CTR_CONFIG_ITEM_INT("counter-duration-inactive", m_config_interim.counter_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds.", 2),
	CTR_CONFIG_ITEM_INT("counter-cooldown-time", m_config_interim.counter_cooldown_time, 0, 60000, "Get/Set counter cooldown time in milliseconds.", 10),
	CTR_CONFIG_ITEM_INT("analog-interval-sample", m_config_interim.analog_interval_sample, 1, 86400, "Get/Set analog sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("analog-interval-aggreg", m_config_interim.analog_interval_aggreg, 1, 86400, "Get/Set analog aggregation interval in seconds.", 300),
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	CTR_CONFIG_ITEM_INT("hygro-interval-sample", m_config_interim.hygro_interval_sample, 1, 86400, "Get/Set hygro sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("hygro-interval-aggreg", m_config_interim.hygro_interval_aggreg, 1, 86400, "Get/Set hygro aggreg interval in seconds.", 300),
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	CTR_CONFIG_ITEM_INT("w1-therm-interval-sample", m_config_interim.w1_therm_interval_sample, 1, 86400, "Get/Set ds18b20 sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("w1-therm-interval-aggreg", m_config_interim.w1_therm_interval_aggreg, 1, 86400, "Get/Set ds18b20 aggreg interval in seconds .", 300),
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

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
