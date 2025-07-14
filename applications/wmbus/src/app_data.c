/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>

struct app_data g_app_data = {
#if defined(FEATURE_SUBSYSTEM_BATT)
	.system_voltage_rest = NAN,
	.system_voltage_load = NAN,
	.system_current_load = NAN,
#endif /* defined(FEATURE_SUBSYSTEM_BATT) */
#if defined(FEATURE_SUBSYSTEM_ACCEL)
	.accel_acceleration_x = NAN,
	.accel_acceleration_y = NAN,
	.accel_acceleration_z = NAN,
	.accel_orientation = INT_MAX,
#endif /* defined(FEATURE_SUBSYSTEM_ACCEL) */
	.therm_temperature = NAN,
};

atomic_t g_app_data_antenna_dual = false;
atomic_t g_app_data_scan_transaction = 0;
atomic_t g_app_data_send_index = 0;

/* Control triggers/flags */
atomic_t g_app_data_scan_trigger = false;
atomic_t g_app_data_working_flag = false;
atomic_t g_app_data_send_flag = false;

int64_t g_app_data_scan_start_timestamp;
int64_t g_app_data_scan_stop_timestamp;

static K_MUTEX_DEFINE(m_lock);

void app_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}