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

/*
 * Keep `g_app_data` zero-initialized so it is placed in `.bss` (RAM only).
 * Non-zero initializers (NAN, INT_MAX) would move the whole structure to
 * `.data`, which occupies the same amount of FLASH and is copied on boot.
 * Non-zero defaults are set at runtime in `app_data_init()` instead.
 */
struct app_data g_app_data;

static K_MUTEX_DEFINE(m_lock);

int app_data_init(void)
{
	app_data_lock();

	g_app_data.accel_acceleration_x = NAN;
	g_app_data.accel_acceleration_y = NAN;
	g_app_data.accel_acceleration_z = NAN;
	g_app_data.accel_orientation = INT_MAX;
	g_app_data.therm_temperature = NAN;

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		g_app_data.ble_tag.sensor[i].last_sample_temperature = NAN;
		g_app_data.ble_tag.sensor[i].last_sample_humidity = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
		g_app_data.soil_sensor.sensor[i].last_temperature = NAN;
		g_app_data.soil_sensor.sensor[i].last_moisture = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

	app_data_unlock();

	return 0;
}

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
