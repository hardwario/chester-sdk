/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include "feature.h"

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
#include <chester/ctr_ble_tag.h>
#include <chester/application/ctr_data.h>
#endif

/* Standard includes */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DATA_MAX_SAMPLES 32

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
#define APP_DATA_MAX_BLE_TAG_MEASUREMENTS 16
#define APP_DATA_MAX_BLE_TAG_SAMPLES      16
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

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
	float samples_temperature[APP_DATA_MAX_BLE_TAG_SAMPLES];
	float samples_humidity[APP_DATA_MAX_BLE_TAG_SAMPLES];

	int measurement_count;
	struct app_data_ble_tag_measurement measurements[APP_DATA_MAX_BLE_TAG_MEASUREMENTS];
};

struct app_data_ble_tag {
	struct app_data_ble_tag_sensor sensor[CTR_BLE_TAG_COUNT];

	int64_t timestamp;
};

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

/* Global application data - flat for system, nested for subsystems */
struct app_data {
	/* System measurements - FLAT (standard CHESTER pattern) */
	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

	/* Generic Modbus device data */
	struct {
		uint8_t slave_addr;
		bool is_online;
		uint32_t last_seen;
		uint32_t error_count;
		uint16_t holding_regs[32];
		uint16_t input_regs[32];
		float values[APP_DATA_MAX_SAMPLES];
		int sample_count;
	} modbus_devices[4];
	int modbus_device_count;

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
