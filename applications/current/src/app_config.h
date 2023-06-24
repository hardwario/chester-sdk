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

#define APP_CONFIG_CHANNEL_COUNT 4

#define APP_CONFIG_INTERVAL_REPORT_MIN 30
#define APP_CONFIG_INTERVAL_REPORT_MAX 86400

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

struct app_config {
	enum app_config_mode mode;

	int interval_report;

#if defined(CONFIG_SHIELD_CTR_Z)
	int event_report_delay;
	int event_report_rate;
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)
	int channel_interval_sample;
	int channel_interval_aggreg;
	bool channel_active[APP_CONFIG_CHANNEL_COUNT];
	bool channel_differential[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_x0[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_y0[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_x1[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_y1[APP_CONFIG_CHANNEL_COUNT];
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	int w1_therm_interval_sample;
	int w1_therm_interval_aggreg;
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */
};

extern struct app_config g_app_config;

int app_config_get_interval_report(void);
int app_config_set_interval_report(int value);

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv);

#if defined(CONFIG_SHIELD_CTR_Z)
int app_config_cmd_config_event_report_delay(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_event_report_rate(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_backup_report_connected(const struct shell *shell, size_t argc,
						  char **argv);
int app_config_cmd_config_backup_report_disconnected(const struct shell *shell, size_t argc,
						     char **argv);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)
int app_config_cmd_config_channel_interval_sample(const struct shell *shell, size_t argc,
						  char **argv);
int app_config_cmd_config_channel_interval_aggreg(const struct shell *shell, size_t argc,
						  char **argv);
int app_config_cmd_config_channel_active(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_differential(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_x0(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_y0(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_x1(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_y1(const struct shell *shell, size_t argc, char **argv);
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
int app_config_cmd_config_w1_therm_interval_sample(const struct shell *shell, size_t argc,
						   char **argv);
int app_config_cmd_config_w1_therm_interval_aggreg(const struct shell *shell, size_t argc,
						   char **argv);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
