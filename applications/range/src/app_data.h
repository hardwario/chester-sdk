/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#define APP_DATA_MAX_MEASUREMENTS  32
#define APP_DATA_MAX_SAMPLES       32
#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_HYGRO_EVENTS  32

#if defined(CONFIG_SHIELD_CTR_DS18B20)
#define APP_DATA_W1_THERM_COUNT       10
#define APP_DATA_W1_THERM_MAX_SAMPLES 128
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#ifdef _cplusplus
extern "C" {
#endif

#if defined(CONFIG_SHIELD_CTR_Z)
struct app_data_backup_event {
	int64_t timestamp;
	bool connected;
};

struct app_data_backup {
	float line_voltage;
	float battery_voltage;
	bool line_present;
	int event_count;
	struct app_data_backup_event events[APP_DATA_MAX_BACKUP_EVENTS];
};
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

struct app_data_aggreg {
	float min;
	float max;
	float avg;
	float mdn;
};

#if defined(CONFIG_SHIELD_CTR_MB7066_A) || defined(CONFIG_SHIELD_CTR_MB7066_B)
struct app_data_sonar {
	int sample_count;
	float samples[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_aggreg measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};
#endif /* defined(CONFIG_SHIELD_CTR_MB7066_A) || defined(CONFIG_SHIELD_CTR_MB7066_B) */

#if defined(CONFIG_SHIELD_CTR_S2)

enum app_data_hygro_event_type {
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_ACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_DEACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_ACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_DEACTIVATED,
};

struct app_data_hygro_event {
	int64_t timestamp;
	enum app_data_hygro_event_type type;
	float value;
};

struct app_data_hygro_measurement {
	struct app_data_aggreg temperature;
	struct app_data_aggreg humidity;
};

struct app_data_hygro {
	int event_count;
	struct app_data_hygro_event events[APP_DATA_MAX_HYGRO_EVENTS];

	bool alarm_hi_active;
	bool alarm_lo_active;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];
	float samples_humidity[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_hygro_measurement measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
struct app_data_w1_therm_measurement {
	struct app_data_aggreg temperature;
};

struct app_data_w1_therm_sensor {
	uint64_t serial_number;

	float last_sample_temperature;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_w1_therm_measurement measurements[APP_DATA_MAX_MEASUREMENTS];
};

struct app_data_w1_therm {
	struct app_data_w1_therm_sensor sensor[APP_DATA_W1_THERM_COUNT];

	int64_t timestamp;
	atomic_t sample;
	atomic_t aggreg;
};
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

struct app_data {
	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

#if defined(CONFIG_SHIELD_CTR_MB7066_A) || defined(CONFIG_SHIELD_CTR_MB7066_B)
	struct app_data_sonar sonar;
#endif /* defined(CONFIG_SHIELD_CTR_MB7066_A) || defined(CONFIG_SHIELD_CTR_MB7066_B) */

#if defined(CONFIG_SHIELD_CTR_Z)
	struct app_data_backup backup;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_S2)
	struct app_data_hygro hygro;
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	struct app_data_w1_therm w1_therm;
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */
};

extern struct app_data g_app_data;

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef _cplusplus
}
#endif

#endif /* APP_DATA_H_ */
