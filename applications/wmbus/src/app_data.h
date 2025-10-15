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

	atomic_t antenna_dual;
	atomic_t scan_transaction;
	atomic_t send_index;

	/* Control triggers/flags */
	atomic_t scan_trigger;
	atomic_t working_flag;
	atomic_t send_flag;

	atomic_t enroll_mode;
	int enroll_timeout;
	int enroll_rssi_threshold;

	int64_t scan_start_timestamp;
	int64_t scan_stop_timestamp;
	bool scan_all;
};

extern struct app_data g_app_data;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
