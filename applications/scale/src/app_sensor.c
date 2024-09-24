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
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_batt.h>
#include <chester/drivers/ctr_z.h>

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

LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG);

static int compare(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;

	return (fa > fb) - (fa < fb);
}

static void aggreg(float *samples, size_t count, float *min, float *max, float *avg, float *mdn)
{
	*min = NAN;
	*max = NAN;
	*avg = NAN;
	*mdn = NAN;

	if (!count) {
		return;
	}

	for (size_t i = 0; i < count; i++) {
		if (isnan(samples[i])) {
			return;
		}
	}

	qsort(samples, count, sizeof(float), compare);

	*min = samples[0];
	*max = samples[count - 1];

	double avg_ = 0;
	for (size_t i = 0; i < count; i++) {
		avg_ += samples[i];
	}
	avg_ /= count;

	*avg = avg_;
	*mdn = samples[count / 2];
}

__unused static void aggreg_sample(float *samples, size_t count, struct app_data_aggreg *sample)
{
	aggreg(samples, count, &sample->min, &sample->max, &sample->avg, &sample->mdn);
}

int app_sensor_sample(void)
{
	int ret;
	float therm_temperature;

	ret = ctr_therm_read(&therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		therm_temperature = NAN;
	}

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

	app_data_lock();
	g_app_data.therm_temperature = therm_temperature;
	g_app_data.accel_acceleration_x = accel_acceleration_x;
	g_app_data.accel_acceleration_y = accel_acceleration_y;
	g_app_data.accel_acceleration_z = accel_acceleration_z;
	g_app_data.accel_orientation = accel_orientation;
	app_data_unlock();

	return 0;
}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
int app_sensor_ble_tag_sample(void)
{
	int ret;

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (g_app_data.ble_tag.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			uint8_t addr[BT_ADDR_SIZE];
			int rssi;
			float voltage;
			float temperature;
			float humidity;
			bool valid;

			ret = ctr_ble_tag_read(i, addr, &rssi, &voltage, &temperature, &humidity,
					       &valid);
			if (ret && ret != -ENOENT) {
				LOG_ERR("Call `ctr_ble_tag_read` failed: %d", ret);
				continue;
			} else if (ret == -ENOENT) {
				continue;
			}

			app_data_lock();
			struct app_data_ble_tag_sensor *sensor = &g_app_data.ble_tag.sensor[i];
			memcpy(sensor->addr, addr, BT_ADDR_SIZE);
			sensor->rssi = rssi;
			sensor->voltage = voltage;
			sensor->last_sample_temperature = temperature;
			sensor->last_sample_humidity = humidity;
			sensor->samples_temperature[sensor->sample_count] = temperature;
			sensor->samples_humidity[sensor->sample_count] = humidity;
			sensor->sample_count++;

			LOG_INF("Sample count: %d", sensor->sample_count);
			app_data_unlock();
		} else {
			LOG_WRN("Sample buffer full");
			return -ENOSPC;
		}
	}

	return 0;
}

int app_sensor_ble_tag_aggreg(void)
{
	int ret;

	struct app_data_ble_tag *ble_tag = &g_app_data.ble_tag;

	app_data_lock();

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (ble_tag->sensor[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&ble_tag->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}

		if (ble_tag->sensor[i].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
			struct app_data_ble_tag_measurement *measurement =
				&ble_tag->sensor[i]
					 .measurements[ble_tag->sensor[i].measurement_count];

			aggreg_sample(ble_tag->sensor[i].samples_temperature,
				      ble_tag->sensor[i].sample_count, &measurement->temperature);

			aggreg_sample(ble_tag->sensor[i].samples_humidity,
				      ble_tag->sensor[i].sample_count, &measurement->humidity);

			ble_tag->sensor[i].measurement_count++;

		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		ble_tag->sensor[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_ble_tag_clear(void)
{
	app_data_lock();

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		g_app_data.ble_tag.sensor[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
