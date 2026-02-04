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
enum app_config_meteo_type {
	APP_CONFIG_METEO_NONE = 0,
	APP_CONFIG_METEO_LAMBRECHT = 1,
	APP_CONFIG_METEO_SENSECAP_S1000 = 2,
	APP_CONFIG_METEO_SENSECAP_S500 = 3,
};

enum app_config_pm_type {
	APP_CONFIG_PM_NONE = 0,
	APP_CONFIG_PM_CUBIC_6303 = 1,
};

enum app_config_modbus_parity {
	APP_CONFIG_MODBUS_PARITY_NONE = 0,
	APP_CONFIG_MODBUS_PARITY_ODD = 1,
	APP_CONFIG_MODBUS_PARITY_EVEN = 2,
};

enum app_config_modbus_stop_bits {
	APP_CONFIG_MODBUS_STOP_BITS_1 = 1, // Stop bits 1
	APP_CONFIG_MODBUS_STOP_BITS_2 = 3, // Stop bits 2
};
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

struct app_config {
	int interval_sample;
	int interval_aggreg;
	int interval_report;
	int interval_poll;
	int downlink_wdg_interval;

#if defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_S2)
	int event_report_delay;
	int event_report_rate;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	bool hygro_t_alarm_hi_report;
	bool hygro_t_alarm_lo_report;
	float hygro_t_alarm_hi_thr;
	float hygro_t_alarm_hi_hst;
	float hygro_t_alarm_lo_thr;
	float hygro_t_alarm_lo_hst;
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

	enum app_config_mode mode;

	/* ### Preserved code "struct variables" (begin) */
	enum app_config_meteo_type meteo_type;
	enum app_config_pm_type pm_type;
	int meteo_addr;
	int pm_addr;
	int modbus_baud;
	int modbus_addr;
	enum app_config_modbus_parity modbus_parity;
	enum app_config_modbus_stop_bits modbus_stop_bits;
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
