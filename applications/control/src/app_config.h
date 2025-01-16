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

/* ### Preserved code "includes" (begin) */
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
};

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

enum app_config_channel_mode {
	APP_CONFIG_CHANNEL_MODE_TRIGGER = 0,
	APP_CONFIG_CHANNEL_MODE_COUNTER = 1,
	APP_CONFIG_CHANNEL_MODE_VOLTAGE = 2,
	APP_CONFIG_CHANNEL_MODE_CURRENT = 3,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

enum app_config_input_type {
	APP_CONFIG_INPUT_TYPE_NPN = 0,
	APP_CONFIG_INPUT_TYPE_PNP = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

struct app_config {
	int interval_report;
	int interval_poll;
	int downlink_wdg_interval;

#if defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X0_A)
	int event_report_delay;
	int event_report_rate;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	enum app_config_channel_mode channel_mode_1;
	enum app_config_channel_mode channel_mode_2;
	enum app_config_channel_mode channel_mode_3;
	enum app_config_channel_mode channel_mode_4;
	enum app_config_input_type trigger_input_type;
	enum app_config_input_type counter_input_type;
	int trigger_duration_active;
	int trigger_duration_inactive;
	int trigger_cooldown_time;
	bool trigger_report_active;
	bool trigger_report_inactive;
	int counter_interval_aggreg;
	int counter_duration_active;
	int counter_duration_inactive;
	int counter_cooldown_time;
	int analog_interval_sample;
	int analog_interval_aggreg;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	int hygro_interval_sample;
	int hygro_interval_aggreg;
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	int w1_therm_interval_sample;
	int w1_therm_interval_aggreg;
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

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
