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
#include <chester/ctr_adc.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
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

LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG);

#define BATT_TEST_INTERVAL_MSEC (12 * 60 * 60 * 1000)

static int compare(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;

	return (fa > fb) - (fa < fb);
}

__unused static void aggreg(float *samples, size_t count, float *minimum, float *maximum,
			    float *average, float *median)
{
	if (count < 1) {
		*minimum = NAN;
		*maximum = NAN;
		*average = NAN;
		*median = NAN;

		return;
	}

	qsort(samples, count, sizeof(float), compare);

	*minimum = samples[0];
	*maximum = samples[count - 1];

	double average_ = 0;
	for (size_t i = 0; i < count; i++) {
		average_ += samples[i];
	}
	average_ /= count;

	*average = average_;
	*median = samples[count / 2];
}

#if defined(CONFIG_SHIELD_CTR_X0_A)

__unused static void aggreg_sample(float *samples, size_t count, struct app_data_aggreg *sample)
{
	aggreg(samples, count, &sample->min, &sample->max, &sample->avg, &sample->mdn);
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

int app_sensor_sample(void)
{
	int ret;

	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

	ret = ctr_therm_read(&therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		therm_temperature = NAN;
	}

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

#if defined(CONFIG_SHIELD_CTR_X0_A)

void app_sensor_trigger_clear(void)
{
	app_data_lock();

	g_app_data.trigger.event_count = 0;

	app_data_unlock();
}

int app_sensor_counter_aggreg(void)
{
	int ret;
	struct app_data_counter *counter = &g_app_data.counter;

	app_data_lock();

	if (!counter->measurement_count) {
		ret = ctr_rtc_get_ts(&counter->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (counter->measurement_count < APP_DATA_COUNTER_MAX_MEASUREMENTS) {
		struct app_data_counter_measurement *measurement =
			&counter->measurements[counter->measurement_count];

		measurement->value = counter->value;
		counter->measurement_count++;

		LOG_INF("Measurement count: %d", counter->measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

void app_sensor_counter_clear(void)
{
	app_data_lock();

	g_app_data.counter.measurement_count = 0;

	app_data_unlock();
}

int static aggreg_analog(struct app_data_analog *analog)
{
	int ret;

	app_data_lock();

	if (analog->measurement_count == 0) {
		ret = ctr_rtc_get_ts(&analog->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (analog->measurement_count < APP_DATA_ANALOG_MAX_MEASUREMENTS) {
		struct app_data_aggreg *measurement =
			&analog->measurements[analog->measurement_count];

		aggreg_sample(analog->samples, analog->sample_count, measurement);

		analog->measurement_count++;

		LOG_INF("Measurement count: %d", analog->measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	analog->sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_voltage_sample(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.voltage.sample_count < APP_DATA_ANALOG_MAX_SAMPLES) {
		float *sample = &g_app_data.voltage.samples[g_app_data.voltage.sample_count];
		app_data_unlock();

		uint16_t adc_voltage;
		ret = ctr_adc_read(CTR_ADC_CHANNEL_A2, &adc_voltage);

		app_data_lock();

		if (!ret) {

			*sample = (float)CTR_ADC_MILLIVOLTS(adc_voltage) * (10.f + 1.f) / 1000.f;
			g_app_data.voltage.sample_count++;

			LOG_INF("Voltage: %.2f V", *sample);
			LOG_INF("Sample count: %d", g_app_data.voltage.sample_count);

		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			*sample = NAN;
			g_app_data.voltage.sample_count++;
		}

		app_data_unlock();

	} else {
		LOG_WRN("Samples full");
		app_data_unlock();
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_voltage_aggreg(void)
{
	int ret;

	ret = aggreg_analog(&g_app_data.voltage);
	if (ret) {
		LOG_WRN("Call `aggreg_analog` failed: %d", ret);
		return ret;
	}

	return 0;
}

void app_sensor_voltage_clear(void)
{
	app_data_lock();

	g_app_data.voltage.measurement_count = 0;

	app_data_unlock();
}

int app_sensor_current_sample(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.current.sample_count < APP_DATA_ANALOG_MAX_SAMPLES) {
		float *sample = &g_app_data.current.samples[g_app_data.current.sample_count];
		app_data_unlock();

		uint16_t adc_current;
		ret = ctr_adc_read(CTR_ADC_CHANNEL_A3, &adc_current);

		app_data_lock();

		if (!ret) {

			*sample = CTR_ADC_X0_CL_MILLIAMPS(adc_current);
			g_app_data.current.sample_count++;

			LOG_INF("Current: %.2f mA", *sample);
			LOG_INF("Sample count: %d", g_app_data.current.sample_count);

		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			*sample = NAN;
			g_app_data.current.sample_count++;
		}
		app_data_unlock();

	} else {
		LOG_WRN("Samples full");
		app_data_unlock();
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_current_aggreg(void)
{
	int ret;

	ret = aggreg_analog(&g_app_data.current);
	if (ret) {
		LOG_WRN("Call `aggreg_analog` failed: %d", ret);
		return ret;
	}

	return 0;
}

void app_sensor_current_clear(void)
{
	app_data_lock();

	g_app_data.current.measurement_count = 0;

	app_data_unlock();
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)

int app_sensor_hygro_sample(void)
{
	int ret;

	float temperature;
	float humidity;
	ret = ctr_hygro_read(&temperature, &humidity);
	if (ret) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		temperature = NAN;
		humidity = NAN;
	} else {
		LOG_INF("Temperature: %.2f °C", temperature);
		LOG_INF("Humidity: %.2f %% RH", humidity);
	}

	app_data_lock();

	if (g_app_data.hygro.sample_count < APP_DATA_HYGRO_MAX_SAMPLES) {
		g_app_data.hygro.samples_temperature[g_app_data.hygro.sample_count] = temperature;
		g_app_data.hygro.samples_humidity[g_app_data.hygro.sample_count] = humidity;
		g_app_data.hygro.sample_count++;

		LOG_INF("Sample count: %d", g_app_data.hygro.sample_count);
	} else {
		LOG_WRN("Samples full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_hygro_aggreg(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.hygro.measurement_count == 0) {
		ret = ctr_rtc_get_ts(&g_app_data.hygro.timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (g_app_data.hygro.measurement_count < APP_DATA_HYGRO_MAX_MEASUREMENTS) {
		struct app_data_hygro_measurement *hygro_measurement =
			&g_app_data.hygro.measurements[g_app_data.hygro.measurement_count];

		aggreg_sample(g_app_data.hygro.samples_temperature, g_app_data.hygro.sample_count,
			      &hygro_measurement->temperature);

		aggreg_sample(g_app_data.hygro.samples_humidity, g_app_data.hygro.sample_count,
			      &hygro_measurement->humidity);

		g_app_data.hygro.measurement_count++;

		LOG_INF("Measurement count: %d", g_app_data.hygro.measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	g_app_data.hygro.sample_count = 0;

	app_data_unlock();

	return 0;
}

void app_sensor_hygro_clear(void)
{
	app_data_lock();

	g_app_data.hygro.measurement_count = 0;

	app_data_unlock();
}

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

int app_sensor_w1_therm_sample(void)
{
	int ret;
	uint64_t serial_number;

	for (int i = 0; i < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		if (g_app_data.w1_therm.sensor[i].sample_count < APP_DATA_W1_THERM_MAX_SAMPLES) {
			float temperature = NAN;
			ret = ctr_ds18b20_read(i, &serial_number, &temperature);

			if (ret) {
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", temperature);
			}

			app_data_lock();
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];
			sensor->last_sample_temperature = temperature;
			sensor->samples_temperature[sensor->sample_count] = temperature;
			sensor->serial_number = serial_number;
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

int app_sensor_w1_therm_aggreg(void)
{
	int ret;

	struct app_data_w1_therm *w1_therm = &g_app_data.w1_therm;

	app_data_lock();

	for (int i = 0; i < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		if (w1_therm->sensor[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&w1_therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}
		if (w1_therm->sensor[i].measurement_count < APP_DATA_W1_THERM_MAX_MEASUREMENTS) {
			struct app_data_w1_therm_measurement *measurement =
				&w1_therm->sensor[i]
					 .measurements[w1_therm->sensor[i].measurement_count];

			aggreg_sample(w1_therm->sensor[i].samples_temperature,
				      w1_therm->sensor[i].sample_count, &measurement->temperature);

			w1_therm->sensor[i].measurement_count++;

			LOG_INF("Measurement count: %d", w1_therm->sensor[i].measurement_count);
		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		w1_therm->sensor[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_w1_therm_clear(void)
{
	app_data_lock();

	for (int i = 0; i < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		g_app_data.w1_therm.sensor[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */