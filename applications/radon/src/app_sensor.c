/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_sensor.h"
#include "app_radon.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/ctr_ble_tag.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG);

static int compare(const void *a, const void *b)
{
	int8_t fa = *(const int8_t *)a;
	int8_t fb = *(const int8_t *)b;

	return (fa > fb) - (fa < fb);
}

static void aggreg(float *samples, size_t count, float *min, float *max, float *avg, float *mdn)
{
	qsort(samples, count, sizeof(int8_t), compare);

	*min = samples[0];
	*max = samples[count - 1];

	double avg_ = 0;
	for (size_t i = 0; i < count; i++) {
		avg_ += (double)samples[i];
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

	ret = ctr_therm_read(&g_app_data.therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		g_app_data.therm_temperature = NAN;
	}

	ret = ctr_accel_read(&g_app_data.accel_x, &g_app_data.accel_y, &g_app_data.accel_z,
			     &g_app_data.accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		g_app_data.accel_x = NAN;
		g_app_data.accel_y = NAN;
		g_app_data.accel_z = NAN;
		g_app_data.accel_orientation = INT_MAX;
	}

	return 0;
}

int app_sensor_radon_sample(void)
{
	int ret;

	if (g_app_data.radon.sample_count < APP_DATA_MAX_SAMPLES) {
		app_radon_enable();

		uint16_t hum;
		ret = app_radon_read_humidity(&hum);
		if (ret) {
			LOG_ERR("Call `app_radon_read_humidity` failed: %d", ret);
			return ret;
		}

		LOG_INF("Chamber humidity: %d %%", hum);

		uint16_t temp;
		ret = app_radon_read_temperature(&temp);
		if (ret) {
			LOG_ERR("Call `app_radon_read_temperature` failed: %d", ret);
			return ret;
		}

		app_radon_disable();

		LOG_INF("Chamber temperature: %d C", temp);

		app_data_lock();

		g_app_data.radon.hum_samples[g_app_data.radon.sample_count] = hum;
		g_app_data.radon.temp_samples[g_app_data.radon.sample_count] = temp;
		g_app_data.radon.sample_count++;

		app_data_unlock();

		LOG_INF("Sample count: %d", g_app_data.radon.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_radon_aggreg(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.radon.measurement_count == 0) {
		ret = ctr_rtc_get_ts(&g_app_data.radon.timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	struct app_data_radon *radon = &g_app_data.radon;

	if (radon->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		aggreg_sample(radon->temp_samples, radon->sample_count,
			      &radon->temp_measurements[radon->measurement_count]);
		aggreg_sample(radon->hum_samples, radon->sample_count,
			      &radon->hum_measurements[radon->measurement_count]);

		app_radon_enable();

		ret = app_radon_read_concentration(
			&radon->concentration_measurements[radon->measurement_count], false);
		if (ret) {
			LOG_ERR("Call `app_radon_read_concentration` failed: %d", ret);
			return ret;
		}

		ret = app_radon_read_concentration(
			&radon->daily_concentration_measurements[radon->measurement_count], true);
		if (ret) {
			LOG_ERR("Call `app_radon_read_concentration` failed: %d", ret);
			return ret;
		}

		app_radon_disable();

		radon->measurement_count++;
		LOG_INF("Measurements: %d", radon->measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	radon->sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_radon_clear(void)
{
	app_data_lock();

	g_app_data.radon.measurement_count = 0;

	app_data_unlock();

	return 0;
}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
int app_sensor_ble_tag_sample(void)
{
	int ret;

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		struct app_data_ble_tag_sensor *sensor = &g_app_data.ble_tag.sensor[i];
		sensor->rssi = INT_MAX;
		sensor->voltage = NAN;

		if (g_app_data.ble_tag.sensor[i].sample_count < APP_DATA_MAX_BLE_TAG_SAMPLES) {
			uint8_t addr[BT_ADDR_SIZE];
			int rssi;
			float voltage;
			float temperature;
			float humidity;
			int16_t sensor_mask;
			bool valid;

			ret = ctr_ble_tag_read_cached(i, addr, &rssi, &voltage, &temperature,
						      &humidity, NULL, NULL, NULL, NULL, NULL, NULL,
						      &sensor_mask, &valid);
			if (ret) {
				if (ret == -ENOENT) {
					continue; /* skip no configured sensor */
				}
				valid = false;
				LOG_ERR("Call `ctr_ble_tag_read_cached` failed: %d", ret);
			}

			app_data_lock();

			if (valid) {
				memcpy(sensor->addr, addr, BT_ADDR_SIZE);

				sensor->last_sample_temperature = temperature;
				sensor->last_sample_humidity = humidity;
				if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_RSSI) {
					sensor->rssi = rssi;
				}

				if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
					sensor->voltage = voltage;
				}

				sensor->samples_temperature[sensor->sample_count] = temperature;
				sensor->samples_humidity[sensor->sample_count] = humidity;
				sensor->sample_count++;

				LOG_INF("Sensor %d: sample count: %d", i, sensor->sample_count);
			} else {
				sensor->last_sample_temperature = NAN;
				sensor->last_sample_humidity = NAN;
			}

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

		if (ble_tag->sensor[i].measurement_count < APP_DATA_MAX_BLE_TAG_SAMPLES) {
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
