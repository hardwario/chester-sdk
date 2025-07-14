/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DATA_MAX_BACKUP_EVENTS 32

struct app_data {
	float therm_temperature;
#if defined(FEATURE_SUBSYSTEM_BATT)
	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
#endif /* defined(FEATURE_SUBSYSTEM_BATT) */
#if defined(FEATURE_SUBSYSTEM_ACCEL)
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;
#endif /* defined(FEATURE_SUBSYSTEM_ACCEL) */
};

extern struct app_data g_app_data;

extern atomic_t g_app_data_season_active;
extern atomic_t g_app_data_antenna_dual;
extern atomic_t g_app_data_scan_transaction;
extern atomic_t g_app_data_send_index;

/* Control triggers/flags */
extern atomic_t g_app_data_scan_trigger;
extern atomic_t g_app_data_working_flag;
extern atomic_t g_app_data_send_flag;

extern int64_t g_app_data_scan_start_timestamp;
extern int64_t g_app_data_scan_stop_timestamp;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */