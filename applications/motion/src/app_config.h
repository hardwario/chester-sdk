/*
 * Copyright (c) 2025 HARDWARIO a.s.
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

enum app_config_sensitivity {
	APP_CONFIG_SENSITIVITY_LOW = 0,
	APP_CONFIG_SENSITIVITY_MEDIUM = 1,
	APP_CONFIG_SENSITIVITY_HIGH = 2,
	APP_CONFIG_SENSITIVITY_INDIVIDUAL = 3,
};

struct app_config {
	int interval_sample;
	int interval_report;
	int interval_poll;
	enum app_config_sensitivity sensitivity;
	int motion_sens;
	int motion_blind;
	int motion_pulse;
	int motion_window;
	bool service_mode_enabled;

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
