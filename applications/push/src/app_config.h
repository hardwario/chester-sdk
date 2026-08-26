/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ### Preserved code "includes" (begin) */
#include "app_data.h" /* for APP_DATA_BUTTON_COUNT, reused below */
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

#if defined(FEATURE_HARDWARE_CHESTER_Z)

enum app_config_led_mode {
	APP_CONFIG_LED_MODE_MULTIPLE = 0,
	APP_CONFIG_LED_MODE_SINGLE = 1,
};

#define APP_CONFIG_BUTTON_COLOR_STR_SIZE 16 /* "RRGGBB,RRGGBB" + NUL, with room to spare */

/* Full 24-bit color (8-bit brightness per R/G/B channel) -- CHESTER Z's LED
 * driver takes an arbitrary 8-bit PWM brightness per channel, not just a few
 * fixed presets, so button colors aren't limited to on/off per channel. */
struct app_config_led_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

struct app_config {
	int interval_sample;
	int interval_aggreg;
	int interval_report;
	int interval_poll;
	int downlink_wdg_interval;

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	int event_report_delay;
	int event_report_rate;
	bool backup_report_connected;
	bool backup_report_disconnected;
	enum app_config_led_mode led_mode;
	int led_timeout;
	char button_color_str[APP_DATA_BUTTON_COUNT][APP_CONFIG_BUTTON_COLOR_STR_SIZE];
	struct app_config_led_color button_click_color[APP_DATA_BUTTON_COUNT];
	struct app_config_led_color button_hold_color[APP_DATA_BUTTON_COUNT];
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

	enum app_config_mode mode;

	/* ### Preserved code "struct variables" (begin) */
	/* ^^^ Preserved code "struct variables" (end) */
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv);

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv);

/* ### Preserved code "functions" (begin) */
/* ^^^ Preserved code "functions" (end) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
