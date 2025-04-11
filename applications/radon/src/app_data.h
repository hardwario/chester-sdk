/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>
#include <chester/ctr_ble_tag.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_TAMPER_EVENTS 32
#define APP_DATA_MAX_MEASUREMENTS  32
#define APP_DATA_MAX_SAMPLES       32

struct app_data_aggreg {
	float min;
	float max;
	float avg;
	float mdn;
};

#if defined(FEATURE_TAMPER)
struct app_data_tamper_event {
	int64_t timestamp;
	bool activated;
};

struct app_data_tamper {
	bool activated;
	int event_count;
	struct app_data_tamper_event events[APP_DATA_MAX_TAMPER_EVENTS];
};
#endif /* defined(FEATURE_TAMPER) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

#define APP_DATA_MAX_BLE_TAG_MEASUREMENTS 16
#define APP_DATA_MAX_BLE_TAG_SAMPLES      16

struct app_data_ble_tag_measurement {
	struct app_data_aggreg temperature;
	struct app_data_aggreg humidity;
};

struct app_data_ble_tag_sensor {
	uint8_t addr[BT_ADDR_SIZE];

	int8_t rssi;
	float voltage;

	float last_sample_temperature;
	float last_sample_humidity;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_BLE_TAG_SAMPLES];
	float samples_humidity[APP_DATA_MAX_BLE_TAG_SAMPLES];

	int measurement_count;
	struct app_data_ble_tag_measurement measurements[APP_DATA_MAX_BLE_TAG_MEASUREMENTS];
};

struct app_data_ble_tag {
	struct app_data_ble_tag_sensor sensor[CTR_BLE_TAG_COUNT];

	int64_t timestamp;
	atomic_t sample;
	atomic_t aggreg;
};

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
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
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

struct app_data_radon {
	int sample_count;
	float temp_samples[APP_DATA_MAX_SAMPLES];
	float hum_samples[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_aggreg temp_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct app_data_aggreg hum_measurements[APP_DATA_MAX_MEASUREMENTS];
	uint32_t concentration_measurements[APP_DATA_MAX_MEASUREMENTS];
	uint32_t daily_concentration_measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float accel_x;
	float accel_y;
	float accel_z;
	int accel_orientation;

	struct app_data_radon radon;

#if defined(FEATURE_TAMPER)
	struct app_data_tamper tamper;
#endif /* defined(FEATURE_TAMPER) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	struct app_data_backup backup;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	struct app_data_ble_tag ble_tag;
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
};

extern struct app_data g_app_data;
extern struct k_mutex g_app_data_lock;

static inline void app_data_lock(void)
{
	k_mutex_lock(&g_app_data_lock, K_FOREVER);
}

static inline void app_data_unlock(void)
{
	k_mutex_unlock(&g_app_data_lock);
}

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
