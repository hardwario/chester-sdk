/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_sensor.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_measure, LOG_LEVEL_DBG);

int app_sensor_sample(void)
{
#if defined(FEATURE_SUBSYSTEM_THERM)
	int ret;
	float therm_temperature;

	ret = ctr_therm_read(&therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		therm_temperature = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_THERM) */

#if defined(FEATURE_SUBSYSTEM_ACCEL)
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

	ret = ctr_accel_read(&accel_acceleration_x, &accel_acceleration_y, &accel_acceleration_z,
			     &accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		accel_acceleration_x = NAN;
		accel_acceleration_y = NAN;
		accel_acceleration_z = NAN;
		accel_orientation = INT_MAX;
	}
#endif /* defined(FEATURE_SUBSYSTEM_ACCEL) */

	app_data_lock();

#if defined(FEATURE_SUBSYSTEM_THERM)
	g_app_data.therm_temperature = therm_temperature;
#endif /* defined(FEATURE_SUBSYSTEM_THERM) */

#if defined(FEATURE_SUBSYSTEM_ACCEL)
	g_app_data.accel_acceleration_x = accel_acceleration_x;
	g_app_data.accel_acceleration_y = accel_acceleration_y;
	g_app_data.accel_acceleration_z = accel_acceleration_z;
	g_app_data.accel_orientation = accel_orientation;
#endif /* defined(FEATURE_SUBSYSTEM_ACCEL) */

	app_data_unlock();

	return 0;
}