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
#include <stdint.h>

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
/* Pluggable-sensor slots (LD-81 etc.). A sensor claims a power channel + a
 * measurement channel; see app_sensor_drv.h for the type catalog. */
#define APP_SENSORS_MAX            4
#define APP_CONFIG_SENSOR_STR_SIZE 32

struct app_config_sensor {
	uint8_t type_id;    /* 0 = none; catalog type id (app_sensor_drv.h) */
	uint8_t ch_power;   /* 1-based channel; 0 = sensor needs no power */
	uint8_t ch_measure; /* 1-based measurement channel */
};
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

enum app_config_channel_mode {
	APP_CONFIG_CHANNEL_MODE_DISABLED = 0,
	APP_CONFIG_CHANNEL_MODE_TRIGGER = 1,
	APP_CONFIG_CHANNEL_MODE_COUNTER = 2,
	APP_CONFIG_CHANNEL_MODE_VOLTAGE = 3,
	APP_CONFIG_CHANNEL_MODE_CURRENT = 4,
	APP_CONFIG_CHANNEL_MODE_POWER = 5,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

enum app_config_input_type {
	APP_CONFIG_INPUT_TYPE_NPN = 0,
	APP_CONFIG_INPUT_TYPE_PNP = 1,
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

struct app_config {
	int interval_report;
	int interval_poll;
	int downlink_wdg_interval;
	int event_report_delay;
	int event_report_rate;

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	enum app_config_channel_mode channel_mode_1;
	enum app_config_channel_mode channel_mode_2;
	enum app_config_channel_mode channel_mode_3;
	enum app_config_channel_mode channel_mode_4;
	enum app_config_input_type trigger_input_type;
	enum app_config_input_type counter_input_type;
	int trigger_duration_active;
	int trigger_duration_inactive;
	int trigger_cooldown_time;
	bool trigger_report_active;
	bool trigger_report_inactive;
	int counter_interval_aggreg;
	int counter_duration_active;
	int counter_duration_inactive;
	int counter_cooldown_time;
	int analog_interval_sample;
	int analog_interval_aggreg;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	int hygro_interval_sample;
	int hygro_interval_aggreg;
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	int w1_therm_interval_sample;
	int w1_therm_interval_aggreg;
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

	enum app_config_mode mode;

	/* ### Preserved code "struct variables" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	char sensor_str[APP_SENSORS_MAX][APP_CONFIG_SENSOR_STR_SIZE];
	struct app_config_sensor sensors[APP_SENSORS_MAX];
	int ld81_threshold_mv; /* LD-81 flood threshold; flood = mV above this */
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
	/* ^^^ Preserved code "struct variables" (end) */
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv);

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv);

/* ### Preserved code "functions" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
/* Channel mode by 0-based channel index (0..APP_DATA_NUM_CHANNELS-1). */
enum app_config_channel_mode app_config_get_channel_mode(int ch_idx);
/* Sensor slot (0-based) owning a 1-based channel as power/measure, else -1. */
int app_config_channel_owner(int ch_num);
/* True when a parsed slot's channels are usable: in range for this build, and
 * distinct when the driver needs a power channel. Used both when loading a slot
 * from flash (which bypasses the shell path's validation) and before the runtime
 * indexes the channel lookup tables with them. */
bool app_config_sensor_channels_valid(const struct app_config_sensor *s);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "functions" (end) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
