/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_config {
	int active_filter;
	int inactive_filter;
	int report_interval;
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_active_filter(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_inactive_filter(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_report_interval(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
