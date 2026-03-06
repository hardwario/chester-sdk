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
#define APP_CONFIG_INTERVAL_REPORT_MIN 30
#define APP_CONFIG_INTERVAL_REPORT_MAX 86400

/* Common enum values for calibration mode */
#define APP_CONFIG_CALIB_MODE_MEAN 0
#define APP_CONFIG_CALIB_MODE_RMS  1

/* Helper macros to get/set calibration mode by channel index (0-based) */
#if defined(FEATURE_HARDWARE_CHESTER_K1)
#define APP_CONFIG_GET_CALIB_MODE(cfg, idx)                                                        \
	((idx) == 0   ? (cfg).channel_calib_mode_1                                                 \
	 : (idx) == 1 ? (cfg).channel_calib_mode_2                                                 \
	 : (idx) == 2 ? (cfg).channel_calib_mode_3                                                 \
		      : (cfg).channel_calib_mode_4)

#define APP_CONFIG_SET_CALIB_MODE(cfg, idx, val)                                                   \
	do {                                                                                       \
		switch (idx) {                                                                     \
		case 0:                                                                            \
			(cfg).channel_calib_mode_1 = (val);                                        \
			break;                                                                     \
		case 1:                                                                            \
			(cfg).channel_calib_mode_2 = (val);                                        \
			break;                                                                     \
		case 2:                                                                            \
			(cfg).channel_calib_mode_3 = (val);                                        \
			break;                                                                     \
		case 3:                                                                            \
			(cfg).channel_calib_mode_4 = (val);                                        \
			break;                                                                     \
		}                                                                                  \
	} while (0)
#endif
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_CHANNEL_COUNT 4

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

#if defined(FEATURE_HARDWARE_CHESTER_K1)

enum app_config_channel_calib_mode_1 {
	APP_CONFIG_CHANNEL_CALIB_MODE_1_MEAN = 0,
	APP_CONFIG_CHANNEL_CALIB_MODE_1_RMS = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_K1) */

#if defined(FEATURE_HARDWARE_CHESTER_K1)

enum app_config_channel_calib_mode_2 {
	APP_CONFIG_CHANNEL_CALIB_MODE_2_MEAN = 0,
	APP_CONFIG_CHANNEL_CALIB_MODE_2_RMS = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_K1) */

#if defined(FEATURE_HARDWARE_CHESTER_K1)

enum app_config_channel_calib_mode_3 {
	APP_CONFIG_CHANNEL_CALIB_MODE_3_MEAN = 0,
	APP_CONFIG_CHANNEL_CALIB_MODE_3_RMS = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_K1) */

#if defined(FEATURE_HARDWARE_CHESTER_K1)

enum app_config_channel_calib_mode_4 {
	APP_CONFIG_CHANNEL_CALIB_MODE_4_MEAN = 0,
	APP_CONFIG_CHANNEL_CALIB_MODE_4_RMS = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_K1) */

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
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_K1)
	bool channel_active[4];
	bool channel_differential[4];
	float channel_calib_x0[4];
	float channel_calib_x1[4];
	float channel_calib_y0[4];
	float channel_calib_y1[4];
	enum app_config_channel_calib_mode_1 channel_calib_mode_1;
	enum app_config_channel_calib_mode_2 channel_calib_mode_2;
	enum app_config_channel_calib_mode_3 channel_calib_mode_3;
	enum app_config_channel_calib_mode_4 channel_calib_mode_4;
#endif /* defined(FEATURE_HARDWARE_CHESTER_K1) */

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
int app_config_get_interval_report(void);
int app_config_set_interval_report(int value);
#if defined(FEATURE_HARDWARE_CHESTER_K1)
int app_config_save_channel_calib(int channel);
#endif
/* ^^^ Preserved code "functions" (end) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
