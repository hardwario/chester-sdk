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

#if defined(CONFIG_SHIELD_CTR_X0_A)
enum app_config_channel_mode {
	APP_CONFIG_CHANNEL_MODE_TRIGGER = 0,
	APP_CONFIG_CHANNEL_MODE_COUNTER = 1,
	APP_CONFIG_CHANNEL_MODE_VOLTAGE = 2,
	APP_CONFIG_CHANNEL_MODE_CURRENT = 3,
};

enum app_config_input_type {
	APP_CONFIG_INPUT_TYPE_NPN = 0,
	APP_CONFIG_INPUT_TYPE_PNP = 1,
};
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

struct app_config {
	int interval_report;
	int interval_poll;

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	int event_report_delay;
	int event_report_rate;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	bool backup_report_connected;
	bool backup_report_disconnected;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	enum app_config_channel_mode channel_mode_1;
	enum app_config_channel_mode channel_mode_2;
	enum app_config_channel_mode channel_mode_3;
	enum app_config_channel_mode channel_mode_4;

	enum app_config_input_type trigger_input_type;
	int trigger_duration_active;
	int trigger_duration_inactive;
	int trigger_cooldown_time;
	bool trigger_report_active;
	bool trigger_report_inactive;

	int counter_interval_aggreg;
	enum app_config_input_type counter_input_type;
	int counter_duration_active;
	int counter_duration_inactive;
	int counter_cooldown_time;
	int analog_interval_sample;
	int analog_interval_aggreg;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	int hygro_interval_sample;
	int hygro_interval_aggreg;
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	int w1_therm_interval_sample;
	int w1_therm_interval_aggreg;
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */
};

extern struct app_config g_app_config;

/* clang-format off */

#define CONFIG_PARAM_LIST_COMMON() \
	CONFIG_PARAM_INT(interval-report, interval_report, 30, 86400, "Get/Set report interval in seconds (format: <30-86400>).") \
	CONFIG_PARAM_INT(interval-poll, interval_poll, 5, 43200, "Get/Set poll interval in seconds (format: <5-43200>).")

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
#define CONFIG_PARAM_LIST_CTR_X0_Z() \
	CONFIG_PARAM_INT(event-report-delay, event_report_delay, 1, 86400, "Get/Set event report delay in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(event-report-rate, event_report_rate, 1, 3600, "Get/Set event report rate in reports per hour (format: <1-3600>).")
	#else
#define CONFIG_PARAM_LIST_CTR_X0_Z()
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
#define CONFIG_PARAM_LIST_CTR_Z() \
	CONFIG_PARAM_BOOL(backup-report-connected, backup_report_connected, "Get/Set report when backup is active (format: true, false).") \
	CONFIG_PARAM_BOOL(backup-report-disconnected, backup_report_disconnected, "Get/Set report when backup is inactive (format: true, false).")
#else
#define CONFIG_PARAM_LIST_CTR_Z()
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
#define CONFIG_PARAM_LIST_CTR_X0() \
	CONFIG_PARAM_ENUM(channel-mode-1, channel_mode_1, app_config_enum_channel_mode, ARRAY_SIZE(app_config_enum_channel_mode), "Get/Set channel input mode (format: trigger, counter, voltage, current).") \
	CONFIG_PARAM_ENUM(channel-mode-2, channel_mode_2, app_config_enum_channel_mode, ARRAY_SIZE(app_config_enum_channel_mode), "Get/Set channel input mode (format: trigger, counter, voltage, current).") \
	CONFIG_PARAM_ENUM(channel-mode-3, channel_mode_3, app_config_enum_channel_mode, ARRAY_SIZE(app_config_enum_channel_mode), "Get/Set channel input mode (format: trigger, counter, voltage, current).") \
	CONFIG_PARAM_ENUM(channel-mode-4, channel_mode_4, app_config_enum_channel_mode, ARRAY_SIZE(app_config_enum_channel_mode), "Get/Set channel input mode (format: trigger, counter, voltage, current).") \
	CONFIG_PARAM_ENUM(trigger-input-type, trigger_input_type, app_config_enum_trigger_input_type, ARRAY_SIZE(app_config_enum_trigger_input_type), "Get/Set event input type (format: npn, pnp).") \
	CONFIG_PARAM_INT(trigger-duration-active, trigger_duration_active, 0, 60000, "Get/Set event active duration in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_INT(trigger-duration-inactive, trigger_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_INT(trigger-cooldown-time, trigger_cooldown_time, 0, 60000, "Get/Set trigger cooldown time in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_BOOL(trigger-report-active, trigger_report_active, "Get/Set report when trigger is active (format: true, false).") \
	CONFIG_PARAM_BOOL(trigger-report-inactive, trigger_report_inactive, "Get/Set report when trigger is inactive (format: true, false).") \
	CONFIG_PARAM_INT(counter-interval-aggreg, counter_interval_aggreg, 1, 86400, "Get/Set counter aggregation interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_ENUM(counter-input-type, counter_input_type, app_config_enum_trigger_input_type, ARRAY_SIZE(app_config_enum_trigger_input_type), "Get/Set counter input type (format: npn, pnp).") \
	CONFIG_PARAM_INT(counter-duration-active, counter_duration_active, 0, 60000, "Get/Set event active duration in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_INT(counter-duration-inactive, counter_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_INT(counter-cooldown-time, counter_cooldown_time, 0, 60000, "Get/Set counter cooldown time in milliseconds (format: <0-60000>).") \
	CONFIG_PARAM_INT(analog-interval-sample, analog_interval_sample, 1, 86400, "Get/Set analog sample interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(analog-interval-aggreg, analog_interval_aggreg, 1, 86400, "Get/Set analog aggregation interval in seconds (format: <1-86400>).")
#else
#define CONFIG_PARAM_LIST_CTR_X0()
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */


#if defined(CONFIG_SHIELD_CTR_S2)
#define CONFIG_PARAM_LIST_CTR_S2() \
	CONFIG_PARAM_INT(hygro-interval-sample, hygro_interval_sample, 1, 86400, "Get/Set hygro sample interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(hygro-interval-aggreg, hygro_interval_aggreg, 1, 86400, "Get/Set hygro aggreg interval in seconds (format: <1-86400>).")
#else
#define CONFIG_PARAM_LIST_CTR_S2()
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
#define CONFIG_PARAM_LIST_CTR_DS18B20() \
	CONFIG_PARAM_INT(w1-therm-interval-sample, w1_therm_interval_sample, 1, 86400, "Get/Set ds18b20 sample interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(w1-therm-interval-aggreg, w1_therm_interval_aggreg, 1, 86400, "Get/Set ds18b20 aggreg interval in seconds (format: <1-86400>).")
#else
#define CONFIG_PARAM_LIST_CTR_DS18B20()
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

/* clang-format on */

#define CONFIG_PARAM_LIST()                                                                        \
	CONFIG_PARAM_LIST_COMMON()                                                                 \
	CONFIG_PARAM_LIST_CTR_X0_Z()                                                               \
	CONFIG_PARAM_LIST_CTR_Z()                                                                  \
	CONFIG_PARAM_LIST_CTR_X0()                                                                 \
	CONFIG_PARAM_LIST_CTR_S2()                                                                 \
	CONFIG_PARAM_LIST_CTR_DS18B20()

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help)                                      \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help)                                                 \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_ENUM(_name_d, _name_u, _items, _count, _help)                                 \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);

CONFIG_PARAM_LIST();

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL
#undef CONFIG_PARAM_ENUM

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
