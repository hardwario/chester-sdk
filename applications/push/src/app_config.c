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
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ### Preserved code "includes" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_Z)
static int button_color_parse_cb(const struct shell *shell, char *argv,
				  const struct ctr_config_item *item);
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-push"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("interval-sample", m_config_interim.interval_sample, 1, 86400, "Get/Set sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("interval-aggreg", m_config_interim.interval_aggreg, 1, 86400, "Get/Set aggregate interval in seconds.", 300),
	CTR_CONFIG_ITEM_INT("interval-report", m_config_interim.interval_report, 30, 86400, "Get/Set report interval in seconds.", 1800),
	CTR_CONFIG_ITEM_INT("interval-poll", m_config_interim.interval_poll, 0, 86400, "Get/Set poll interval in seconds (disabled if 0).", 0),
	CTR_CONFIG_ITEM_INT("downlink-wdg-interval", m_config_interim.downlink_wdg_interval, 0, 1209600, "Get/Set downlink watchdog interval in seconds (disabled if 0).", 129600),

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	CTR_CONFIG_ITEM_INT("event-report-delay", m_config_interim.event_report_delay, 1, 86400, "Get/Set event report delay in seconds.", 1),
	CTR_CONFIG_ITEM_INT("event-report-rate", m_config_interim.event_report_rate, 1, 3600, "Get/Set event report rate in reports per hour.", 30),
	CTR_CONFIG_ITEM_BOOL("backup-report-connected", m_config_interim.backup_report_connected, "Get/Set report when backup is active.", true),
	CTR_CONFIG_ITEM_BOOL("backup-report-disconnected", m_config_interim.backup_report_disconnected, "Get/Set report when backup is inactive.", true),
	CTR_CONFIG_ITEM_ENUM("led-mode", m_config_interim.led_mode, ((const char*[]){"multiple", "single"}), "Get/Set button LED mode (multiple LEDs lit independently, or a single exclusive LED).", APP_CONFIG_LED_MODE_MULTIPLE),
	CTR_CONFIG_ITEM_INT("led-timeout", m_config_interim.led_timeout, 0, 86400, "Get/Set button LED timeout in seconds (0 = stay lit until cleared by downlink).", 5),
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

	CTR_CONFIG_ITEM_ENUM("mode", m_config_interim.mode, ((const char*[]){"none", "lte", "lrw"}), "Set communication mode", APP_CONFIG_MODE_NONE),

	/* ### Preserved code "config" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_Z)
	/* Per-button LED color: click_color,hold_color, each a 24-bit RRGGBB hex color */
	CTR_CONFIG_ITEM_STRING_PARSE_CB("button-x", m_config_interim.button_color_str[0],
		"Button x: click_color,hold_color (24-bit RRGGBB hex, e.g. 00FF00,FF0000)", "00FF00,FF0000", button_color_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("button-1", m_config_interim.button_color_str[1],
		"Button 1: click_color,hold_color (24-bit RRGGBB hex, e.g. 00FF00,FF0000)", "00FF00,FF0000", button_color_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("button-2", m_config_interim.button_color_str[2],
		"Button 2: click_color,hold_color (24-bit RRGGBB hex, e.g. 00FF00,FF0000)", "00FF00,FF0000", button_color_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("button-3", m_config_interim.button_color_str[3],
		"Button 3: click_color,hold_color (24-bit RRGGBB hex, e.g. 00FF00,FF0000)", "00FF00,FF0000", button_color_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("button-4", m_config_interim.button_color_str[4],
		"Button 4: click_color,hold_color (24-bit RRGGBB hex, e.g. 00FF00,FF0000)", "00FF00,FF0000", button_color_parse_cb),
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */
	/* ^^^ Preserved code "config" (end) */

};
/* clang-format on */

/* ### Preserved code "function" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_Z)

/* Parse exactly 6 hex digits ("RRGGBB") into a 24-bit color. */
static int parse_hex_color(const char *str, struct app_config_led_color *color)
{
	if (strlen(str) != 6) {
		return -EINVAL;
	}

	for (int i = 0; i < 6; i++) {
		if (!isxdigit((unsigned char)str[i])) {
			return -EINVAL;
		}
	}

	char byte_str[3] = {0};
	unsigned long value;

	memcpy(byte_str, &str[0], 2);
	value = strtoul(byte_str, NULL, 16);
	color->r = (uint8_t)value;

	memcpy(byte_str, &str[2], 2);
	value = strtoul(byte_str, NULL, 16);
	color->g = (uint8_t)value;

	memcpy(byte_str, &str[4], 2);
	value = strtoul(byte_str, NULL, 16);
	color->b = (uint8_t)value;

	return 0;
}

/* Parse "click_color,hold_color" (each a 6-hex-digit RRGGBB color, e.g.
 * "00FF00,FF0000") into click/hold colors. */
static int parse_button_color_string(const char *input, struct app_config_led_color *click_color,
				      struct app_config_led_color *hold_color)
{
	char buf[APP_CONFIG_BUTTON_COLOR_STR_SIZE];
	char *saveptr;
	char *part;

	if (!input || !input[0]) {
		return -EINVAL;
	}

	strncpy(buf, input, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	part = strtok_r(buf, ",", &saveptr);
	if (!part || parse_hex_color(part, click_color)) {
		return -EINVAL;
	}

	part = strtok_r(NULL, ",", &saveptr);
	if (!part || parse_hex_color(part, hold_color)) {
		return -EINVAL;
	}

	if (strtok_r(NULL, ",", &saveptr)) {
		return -EINVAL; /* too many parts */
	}

	return 0;
}

/* Button color parse callback - validates and stores string */
static int button_color_parse_cb(const struct shell *shell, char *argv,
				  const struct ctr_config_item *item)
{
	size_t len = strlen(argv);

	if (len >= item->size) {
		shell_error(shell, "Value too long (max %d)", item->size - 1);
		return -ENOMEM;
	}

	struct app_config_led_color click_color, hold_color;
	int ret = parse_button_color_string(argv, &click_color, &hold_color);
	if (ret) {
		shell_error(shell, "Invalid format. Use: click_color,hold_color");
		shell_print(shell, "  Each color is 6 hex digits, RRGGBB (e.g. 00FF00,FF0000)");
		return ret;
	}

	strncpy(item->variable, argv, item->size - 1);
	((char *)item->variable)[item->size - 1] = '\0';

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */
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
#if defined(FEATURE_HARDWARE_CHESTER_Z)
	for (int i = 0; i < APP_DATA_BUTTON_COUNT; i++) {
		if (parse_button_color_string(m_config_interim.button_color_str[i],
					       &m_config_interim.button_click_color[i],
					       &m_config_interim.button_hold_color[i])) {
			LOG_WRN("Invalid button-%d color string, defaulting to off", i);
			m_config_interim.button_click_color[i] =
				(struct app_config_led_color){0, 0, 0};
			m_config_interim.button_hold_color[i] =
				(struct app_config_led_color){0, 0, 0};
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */
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
