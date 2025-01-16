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

struct app_config {
	int interval_report;
	int interval_poll;
	int interval_sample;
	int interval_aggreg;
	int downlink_wdg_interval;
	bool channel_a1_active;
	bool channel_a2_active;
	bool channel_b1_active;
	bool channel_b2_active;
	int weight_measurement_interval;

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	int event_report_delay;
	int event_report_rate;
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER)
	int people_measurement_interval;
	int people_counter_power_off_delay;
	int people_counter_stay_timeout;
	int people_counter_adult_border;
#endif /* defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER) */

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
