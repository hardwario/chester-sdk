/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_send.h"
#include "app_sensor.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_k1.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_measure, LOG_LEVEL_DBG);

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

__unused static void aggreg_sample(float *samples, size_t count,
				   struct app_data_analog_aggreg *sample)
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
		g_app_data.therm_temperature = NAN;
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

#if defined(CONFIG_SHIELD_CTR_K1)

int app_sensor_analog_sample(void)
{
	int ret;
	int err = 0;

	enum ctr_k1_channel channels[APP_CONFIG_CHANNEL_COUNT] = {0};
	struct ctr_k1_calibration calibrations[APP_CONFIG_CHANNEL_COUNT] = {0};

	size_t channels_count = 0;

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		calibrations[i].x0 = NAN;
		calibrations[i].y0 = NAN;
		calibrations[i].x1 = NAN;
		calibrations[i].y1 = NAN;

		if (!g_app_config.channel_active[i]) {
			continue;
		}

		if (g_app_data.channel[i].sample_count >= APP_DATA_ANALOG_MAX_SAMPLES) {
			LOG_WRN("Channel %d measurements buffer full", i);
			continue;
		}

		if (g_app_config.channel_differential[i]) {
			channels[channels_count] = CTR_K1_CHANNEL_DIFFERENTIAL(i + 1);
		} else {
			channels[channels_count] = CTR_K1_CHANNEL_SINGLE_ENDED(i + 1);
		}

		int x0 = g_app_config.channel_calib_x0[i];
		int y0 = g_app_config.channel_calib_y0[i];
		int x1 = g_app_config.channel_calib_x1[i];
		int y1 = g_app_config.channel_calib_y1[i];

		if (x0 || y0 || x1 || y1) {
			if (x1 - x0) {
				calibrations[channels_count].x0 = x0;
				calibrations[channels_count].y0 = y0;
				calibrations[channels_count].x1 = x1;
				calibrations[channels_count].y1 = y1;
			} else {
				LOG_WRN("Channel %u: Invalid calibration settings", i + 1);
			}
		}

		channels_count++;
	}

	if (!channels_count) {
		LOG_WRN("No channels are active");
		return 0;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_k1));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	for (size_t i = 0; i < channels_count; i++) {
		ret = ctr_k1_set_power(dev, channels[i], true);
		if (ret) {
			LOG_ERR("Call `ctr_k1_set_power` (%u) failed: %d", i, ret);
			err = ret;
			goto error;
		}

		if (i != channels_count - 1) {
			k_sleep(K_MSEC(10));
		}
	}

	k_sleep(K_MSEC(100));

	struct ctr_k1_result results[ARRAY_SIZE(channels)] = {0};
	ret = ctr_k1_measure(dev, channels, channels_count, calibrations, results);
	if (ret) {
		LOG_ERR("Call `ctr_k1_measure` failed: %d", ret);
		err = ret;
		goto error;
	}

	app_data_lock();

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		int sample_count = g_app_data.channel[i].sample_count;
		g_app_data.channel[i].samples_mean[sample_count] = NAN;
		g_app_data.channel[i].samples_rms[sample_count] = NAN;
	}

	for (size_t i = 0; i < channels_count; i++) {
		int sample_count = g_app_data.channel[CTR_K1_CHANNEL_IDX(channels[i])].sample_count;
		g_app_data.channel[CTR_K1_CHANNEL_IDX(channels[i])].samples_mean[sample_count] =
			results[i].avg;
		g_app_data.channel[CTR_K1_CHANNEL_IDX(channels[i])].samples_rms[sample_count] =
			results[i].rms;

		LOG_INF("Channel %d: sample count: %d, mean: %.1f, rms: %.1f",
			CTR_K1_CHANNEL_IDX(channels[i]) + 1, sample_count, results[i].avg,
			results[i].rms);
	}

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		g_app_data.channel[i].sample_count++;
	}

	app_data_unlock();

error:

	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		ret = ctr_k1_set_power(dev, channels[i], false);
		if (ret) {
			LOG_ERR("Call `ctr_k1_set_power` (%u) failed: %d", i, ret);
			err = err ? err : ret;
		}

		if (i != channels_count - 1) {
			k_sleep(K_MSEC(10));
		}
	}

	return err;
}

int app_sensor_analog_aggreg(void)
{
	int ret;

	app_data_lock();

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {

		if (!g_app_config.channel_active[i]) {
			continue;
		}

		if (g_app_data.channel[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&g_app_data.channel[i].timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}

		if (g_app_data.channel[i].measurement_count < APP_DATA_ANALOG_MAX_MEASUREMENTS) {

			int measurement_count = g_app_data.channel[i].measurement_count;

			aggreg_sample(g_app_data.channel[i].samples_mean,
				      g_app_data.channel[i].sample_count,
				      &g_app_data.channel[i].measurements_mean[measurement_count]);

			aggreg_sample(g_app_data.channel[i].samples_rms,
				      g_app_data.channel[i].sample_count,
				      &g_app_data.channel[i].measurements_rms[measurement_count]);

			LOG_INF("Channel %d, count: %d", i + 1, measurement_count);

			g_app_data.channel[i].measurement_count++;
		} else {
			LOG_WRN("Measurement buffer full");
		}
	}

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		g_app_data.channel[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_analog_clear(void)
{
	app_data_lock();

	for (int i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		g_app_data.channel[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

int app_sensor_w1_therm_sample(void)
{
	int ret;
	float temperature;
	uint64_t serial_number;

	for (int j = 0; j < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); j++) {
		if (g_app_data.w1_therm.sensor[j].sample_count < APP_DATA_W1_THERM_MAX_SAMPLES) {
			int i = g_app_data.w1_therm.sensor[j].sample_count;

			ret = ctr_ds18b20_read(j, &serial_number, &temperature);

			if (ret) {
				temperature = NAN;
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", temperature);
			}

			app_data_lock();
			g_app_data.w1_therm.sensor[j].last_sample_temperature = temperature;
			g_app_data.w1_therm.sensor[j].samples_temperature[i] = temperature;
			g_app_data.w1_therm.sensor[j].serial_number = serial_number;
			g_app_data.w1_therm.sensor[j].sample_count++;
			app_data_unlock();

			LOG_INF("Sample count: %d", g_app_data.w1_therm.sensor[j].sample_count);
		} else {
			LOG_WRN("Sample buffer full");
			return -ENOSPC;
		}
	}

	return 0;
}

int app_sensor_w1_therm_aggreg(void)
{
	int ret;

	struct app_data_w1_therm *w1_therm = &g_app_data.w1_therm;

	app_data_lock();

	for (int j = 0; j < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); j++) {
		if (w1_therm->sensor[j].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&w1_therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}
		if (w1_therm->sensor[j].measurement_count < APP_DATA_W1_THERM_MAX_MEASUREMENTS) {
			struct app_data_w1_therm_measurement *measurement =
				&w1_therm->sensor[j]
					 .measurements[w1_therm->sensor[j].measurement_count];

			aggreg_sample(w1_therm->sensor[j].samples_temperature,
				      w1_therm->sensor[j].sample_count, &measurement->temperature);

			w1_therm->sensor[j].measurement_count++;

			LOG_INF("Measurement count: %d", w1_therm->sensor[j].measurement_count);
		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		w1_therm->sensor[j].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_w1_therm_clear(void)
{
	app_data_lock();

	for (int j = 0; j < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); j++) {
		g_app_data.w1_therm.sensor[j].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */
