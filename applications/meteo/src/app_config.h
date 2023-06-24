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
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_config {
	int interval_sample;
	int interval_aggreg;
	int interval_report;

#if defined(CONFIG_SHIELD_CTR_Z)
	int event_report_delay;
	int event_report_rate;
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);

int app_config_cmd_config_interval_sample(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_interval_aggreg(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv);

#if defined(CONFIG_SHIELD_CTR_Z)
int app_config_cmd_config_event_report_delay(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_event_report_rate(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_backup_report_connected(const struct shell *shell, size_t argc,
						  char **argv);
int app_config_cmd_config_backup_report_disconnected(const struct shell *shell, size_t argc,
						     char **argv);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
