/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/application/ctr_data.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DATA_MAX_MEASUREMENTS  32
#define APP_DATA_MAX_SAMPLES       32
#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_BUTTON_EVENTS 8

#define APP_DATA_BUTTON_COUNT 5

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
#define APP_DATA_MAX_BLE_TAG_SAMPLES      16
#define APP_DATA_MAX_BLE_TAG_MEASUREMENTS 16
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

enum app_data_button_event_type {
	APP_DATA_BUTTON_EVENT_X_CLICK = 0,
	APP_DATA_BUTTON_EVENT_X_HOLD = 1,
	APP_DATA_BUTTON_EVENT_1_CLICK = 2,
	APP_DATA_BUTTON_EVENT_1_HOLD = 3,
	APP_DATA_BUTTON_EVENT_2_CLICK = 4,
	APP_DATA_BUTTON_EVENT_2_HOLD = 5,
	APP_DATA_BUTTON_EVENT_3_CLICK = 6,
	APP_DATA_BUTTON_EVENT_3_HOLD = 7,
	APP_DATA_BUTTON_EVENT_4_CLICK = 8,
	APP_DATA_BUTTON_EVENT_4_HOLD = 9,
};

struct app_data_button_event {
	int64_t timestamp;
	enum app_data_button_event_type type;
};

struct app_data_button {
	atomic_t click_count;
	atomic_t hold_count;
	int event_count;
	struct app_data_button_event events[APP_DATA_MAX_BUTTON_EVENTS];
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

struct app_data_ble_tag_measurement {
	struct ctr_data_aggreg temperature;
	struct ctr_data_aggreg humidity;
};

struct app_data_ble_tag_sensor {
	uint8_t addr[BT_ADDR_SIZE];

	int8_t rssi;
	float voltage;

	float last_sample_temperature;
	float last_sample_humidity;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];
	float samples_humidity[APP_DATA_MAX_SAMPLES];

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

struct app_data {

	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	struct app_data_backup backup;
	struct app_data_button button[APP_DATA_BUTTON_COUNT];
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	struct app_data_ble_tag ble_tag;
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
};

extern struct app_data g_app_data;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
