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

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

struct app_config {
	enum app_config_mode mode;

	int interval_sample;
	int interval_aggreg;
	int interval_report;
	int interval_poll;

#if defined(CONFIG_SHIELD_CTR_S1)
	float iaq_led_thr_warning;
	float iaq_led_thr_alarm;
	float iaq_led_hst;
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	bool hygro_t_alarm_hi_report;
	bool hygro_t_alarm_lo_report;
	float hygro_t_alarm_hi_thr;
	float hygro_t_alarm_hi_hst;
	float hygro_t_alarm_lo_thr;
	float hygro_t_alarm_lo_hst;
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
	int event_report_delay;
	int event_report_rate;
#endif /* defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) ||                         \
	  defined(CONFIG_SHIELD_CTR_X10) */

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */
};

extern struct app_config g_app_config;

/* clang-format off */

#define CONFIG_PARAM_LIST_COMMON() \
	CONFIG_PARAM_INT(interval-sample, interval_sample, 1, 86400, "Get/Set sample interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(interval-aggreg, interval_aggreg, 1, 86400, "Get/Set aggregate interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(interval-report, interval_report, 30, 86400, "Get/Set report interval in seconds (format: <30-86400>).") \
	CONFIG_PARAM_INT(interval-poll, interval_poll, 30, 86400, "Get/Set poll interval in seconds (format: <30-86400>).")

#if defined(CONFIG_SHIELD_CTR_S1)
#define CONFIG_PARAM_LIST_CTR_S1() \
	CONFIG_PARAM_FLOAT(iaq-led-thr-warning, iaq_led_thr_warning, -40.f, 5000.f, "Get/Set IAQ LED warning threshold (format: <-40.0..5000.0>).") \
	CONFIG_PARAM_FLOAT(iaq-led-thr-alarm, iaq_led_thr_alarm, -40.f, 5000.f, "Get/Set IAQ LED alarm threshold (format: <-40.0..5000.0>).") \
	CONFIG_PARAM_FLOAT(iaq-led-hst, iaq_led_hst, 0.f, 1000.f, "Get/Set IAQ LED hysteresis (format: <0.0..1000.0>).")
#else
#define CONFIG_PARAM_LIST_CTR_S1()
#endif

#if defined(CONFIG_SHIELD_CTR_S2)
#define CONFIG_PARAM_LIST_CTR_S2() \
	CONFIG_PARAM_BOOL(hygro-t-alarm-hi-report, hygro_t_alarm_hi_report, "Get/Set report when hygro high temperature alarm is crossed (format: true, false).") \
	CONFIG_PARAM_BOOL(hygro-t-alarm-lo-report, hygro_t_alarm_lo_report, "Get/Set report when hygro low temperature alarm is crossed (format: true, false).") \
	CONFIG_PARAM_FLOAT(hygro-t-alarm-hi-thr, hygro_t_alarm_hi_thr, -40.f, 125.f, "Get/Set hygro high temperature alarm threshold (format: <-40.0..125.0>).") \
	CONFIG_PARAM_FLOAT(hygro-t-alarm-hi-hst, hygro_t_alarm_hi_hst, 0.f, 100.f, "Get/Set hygro high temperature alarm hysteresis (format: <0.0..100.0>).") \
	CONFIG_PARAM_FLOAT(hygro-t-alarm-lo-thr, hygro_t_alarm_lo_thr, -40.f, 125.f, "Get/Set hygro low temperature alarm threshold (format: <-40.0..125.0>).") \
	CONFIG_PARAM_FLOAT(hygro-t-alarm-lo-hst, hygro_t_alarm_lo_hst, 0.f, 100.f, "Get/Set hygro low temperature alarm hysteresis (format: <0.0..100.0>).")
#else
#define CONFIG_PARAM_LIST_CTR_S2()
#endif

#if defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
#define CONFIG_PARAM_LIST_CTR_S2_Z() \
	CONFIG_PARAM_INT(event-report-delay, event_report_delay, 1, 86400, "Get/Set event report delay in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(event-report-rate, event_report_rate, 1, 3600, "Get/Set event report rate in reports per hour (format: <1-3600>).")
#else
#define CONFIG_PARAM_LIST_CTR_S2_Z()
#endif /* defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
#define CONFIG_PARAM_LIST_CTR_Z() \
	CONFIG_PARAM_BOOL(backup-report-connected, backup_report_connected, "Get/Set report when backup is active (format: true, false).") \
	CONFIG_PARAM_BOOL(backup-report-disconnected, backup_report_disconnected, "Get/Set report when backup is inactive (format: true, false).")
#else
#define CONFIG_PARAM_LIST_CTR_Z()
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

/* clang-format on */

#define CONFIG_PARAM_LIST()                                                                        \
	CONFIG_PARAM_LIST_COMMON()                                                                 \
	CONFIG_PARAM_LIST_CTR_S1()                                                                 \
	CONFIG_PARAM_LIST_CTR_S2()                                                                 \
	CONFIG_PARAM_LIST_CTR_S2_Z()                                                               \
	CONFIG_PARAM_LIST_CTR_Z()

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv);

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help)                                      \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help)                                                 \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);

CONFIG_PARAM_LIST();

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
