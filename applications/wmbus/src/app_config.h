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
#include "wmbus.h"
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
};

enum app_config_scan_mode {
	APP_CONFIG_SCAN_MODE_OFF = 0,
	APP_CONFIG_SCAN_MODE_INTERVAL = 1,
	APP_CONFIG_SCAN_MODE_DAILY = 2,
	APP_CONFIG_SCAN_MODE_WEEKLY = 3,
	APP_CONFIG_SCAN_MODE_MONTHLY = 4,
};

enum app_config_scan_ant {
	APP_CONFIG_SCAN_ANT_SINGLE = 0,
	APP_CONFIG_SCAN_ANT_DUAL = 1,
};

struct app_config {
	int scan_timeout;
	int scan_interval;
	int scan_hour;
	int scan_weekday;
	int scan_day;
	enum app_config_scan_mode scan_mode;
	enum app_config_scan_ant scan_ant;
	int poll_interval;
	int downlink_wdg_interval;

	enum app_config_mode mode;

	/* ### Preserved code "struct variables" (begin) */
	uint32_t address[DEVICE_MAX_COUNT];
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
