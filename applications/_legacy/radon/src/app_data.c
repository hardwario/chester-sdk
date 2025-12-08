/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>

struct app_data g_app_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.accel_x = NAN,
	.accel_y = NAN,
	.accel_z = NAN,
	.accel_orientation = INT_MAX,

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	.ble_tag.sensor[0 ... CTR_BLE_TAG_COUNT - 1] =
		{
			.last_sample_temperature = NAN,
			.last_sample_humidity = NAN,
		},
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
};

K_MUTEX_DEFINE(g_app_data_lock);
