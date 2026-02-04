/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>
#include <chester/application/ctr_data.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>

struct app_data g_app_data = {
	.system_voltage_rest = NAN,
	.system_voltage_load = NAN,
	.system_current_load = NAN,
	.accel_acceleration_x = NAN,
	.accel_acceleration_y = NAN,
	.accel_acceleration_z = NAN,
	.accel_orientation = INT_MAX,
	.therm_temperature = NAN,

#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)
	.sensecap = {0},
	.cubic_pm = {0},
#endif /* defined(FEATURE_HARDWARE_CHESTER_X2_MODBUS_A ) ||                                        \
	  defined(FEATURE_HARDWARE_CHESTER_METEO_M) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	.ble_tag.sensor[0 ... CTR_BLE_TAG_COUNT - 1] =
		{
			.last_sample_temperature = NAN,
			.last_sample_humidity = NAN,
		},
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	.soil_sensor.sensor[0 ... APP_DATA_SOIL_SENSOR_COUNT - 1] =
		{
			.last_temperature = NAN,
			.last_moisture = NAN,
		},
#endif
};

static K_MUTEX_DEFINE(m_lock);

void app_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}
void app_data_add_sample_float(struct ctr_data_aggreg *aggreg_ptr, int sample_count, float sample)
{

	int count = sample_count;

	if (count == 0) {

		aggreg_ptr->min = sample;
		aggreg_ptr->max = sample;
		aggreg_ptr->avg = sample;
		aggreg_ptr->mdn = NAN;
	} else {

		if (sample < aggreg_ptr->min) {
			aggreg_ptr->min = sample;
		}
		if (sample > aggreg_ptr->max) {
			aggreg_ptr->max = sample;
		}

		float old_avg = aggreg_ptr->avg;
		float new_avg = (old_avg * (float)count + sample) / (float)(count + 1);
		aggreg_ptr->avg = new_avg;
	}
}
