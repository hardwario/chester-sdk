/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_work.h"
#include "app_lambrecht.h"
#include "app_sensor.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_adc.h>
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_gpio.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_batt.h>
#include <chester/drivers/ctr_meteo.h>
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
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

	float therm_temperature = NAN;
	ret = ctr_therm_read(&therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
	}

	float accel_acceleration_x = NAN;
	float accel_acceleration_y = NAN;
	float accel_acceleration_z = NAN;
	int accel_orientation = INT_MAX;
	ret = ctr_accel_read(&accel_acceleration_x, &accel_acceleration_y, &accel_acceleration_z,
			     &accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
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

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)

static int meteo_sample_wind_direction(void)
{
	int ret;

	app_data_lock();

	struct app_data_meteo_wind_direction *wd = &g_app_data.meteo.wind_direction;

	if (wd->sample_count < APP_DATA_MAX_SAMPLES) {
		float *sample = &wd->samples[wd->sample_count];

		app_data_unlock();

		static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_meteo_a));

		if (!device_is_ready(dev)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		float wind_direction;
		ret = ctr_meteo_get_wind_direction(dev, &wind_direction);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_wind_direction` failed: %d", ret);
			return ret;
		}

		app_data_lock();

		if (isnan(wind_direction)) {
			*sample = NAN;
			wd->sample_count++;
		} else {
			*sample = wind_direction;
			wd->sample_count++;

			LOG_INF("Wind direction: %.2f deg", (double)*sample);
			LOG_INF("Sample count: %d", wd->sample_count);
		}

		app_data_unlock();

	} else {
		LOG_WRN("Samples full");
		app_data_unlock();
		return -ENOSPC;
	}

	return 0;
}

static int meteo_sample_wind_speed(void)
{
	int ret;

	app_data_lock();

	struct app_data_meteo_wind_speed *ws = &g_app_data.meteo.wind_speed;

	if (ws->sample_count < APP_DATA_MAX_SAMPLES) {
		float *sample = &ws->samples[ws->sample_count];

		static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_meteo_a));

		if (!device_is_ready(dev)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		ret = ctr_meteo_get_wind_speed_and_clear(dev, sample);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_wind_speed_and_clear` failed: %d", ret);
			return ret;
		}

		ws->sample_count++;

		LOG_INF("Wind speed: %.2f m/s", (double)*sample);
		LOG_INF("Sample count: %d", ws->sample_count);

		app_data_unlock();

	} else {
		LOG_WRN("Samples full");
		app_data_unlock();
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_meteo_sample(void)
{
	int ret;

	ret = meteo_sample_wind_direction();
	if (ret) {
		LOG_ERR("Call `meteo_sample_wind_direction` failed: %d", ret);
	}

	ret = meteo_sample_wind_speed();
	if (ret) {
		LOG_ERR("Call `meteo_sample_wind_speed` failed: %d", ret);
	}

	return 0;
}

static int meteo_aggreg_wind_speed(void)
{
	int ret;

	struct app_data_meteo_wind_speed *ws = &g_app_data.meteo.wind_speed;

	app_data_lock();

	if (!ws->measurement_count) {
		ret = ctr_rtc_get_ts(&ws->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (ws->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		aggreg_sample(ws->samples, ws->sample_count,
			      &ws->measurements[ws->measurement_count]);
		ws->measurement_count++;

		LOG_INF("Measurement count: %d", ws->measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	ws->sample_count = 0;

	app_data_unlock();

	return 0;
}

#if !defined(M_PI)
#define M_PI 3.14159265f
#endif
#define DEGREES_TO_RADIANS(angle_degrees) ((angle_degrees) * M_PI / 180.f)
#define RADIANS_TO_DEGREES(angle_radians) ((angle_radians) * 180.f / M_PI)

static float get_average_angle(float *array, size_t array_size)
{
	float sum_sin = 0.f;
	float sum_cos = 0.f;

	for (size_t i = 0; i < array_size; i++) {
		if (isnan(array[i])) {
			return NAN;
		}

		sum_sin += sinf(DEGREES_TO_RADIANS(array[i]));
		sum_cos += cosf(DEGREES_TO_RADIANS(array[i]));
	}

	sum_sin /= array_size;
	sum_cos /= array_size;

	float average = RADIANS_TO_DEGREES(atanf(sum_sin / sum_cos));

	if (sum_cos < 0.f) {
		average += 180.f;
	} else if (sum_sin < 0.f && sum_cos > 0.f) {
		average += 360.f;
	}

	if (average < 0.f) {
		average += 360.f;
	}
	if (average >= 360.f) {
		average -= 360.f;
	}

	return average;
}

static int meteo_aggreg_wind_direction(void)
{
	int ret;

	app_data_lock();

	struct app_data_meteo_wind_direction *wd = &g_app_data.meteo.wind_direction;

	if (!wd->measurement_count) {
		ret = ctr_rtc_get_ts(&wd->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (wd->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		float *measurement = &wd->measurements[wd->measurement_count];

		*measurement = get_average_angle(wd->samples, wd->sample_count);
		wd->measurement_count++;

		LOG_INF("Wind direction: %.2f", (double)*measurement);
		LOG_INF("Measurement count: %d", wd->measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	wd->sample_count = 0;

	app_data_unlock();

	return 0;
}

static int meteo_aggreg_rainfall(void)
{
	int ret;

	struct app_data_meteo_rainfall *rf = &g_app_data.meteo.rainfall;

	app_data_lock();

	if (!rf->measurement_count) {
		ret = ctr_rtc_get_ts(&rf->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (rf->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_meteo_a));

		if (!device_is_ready(dev)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		float rainfall;
		ret = ctr_meteo_get_rainfall_and_clear(dev, &rainfall);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_rainfall_and_clear` failed: %d", ret);
			return ret;
		}

		rf->measurements[rf->measurement_count] = rainfall;
		rf->measurement_count++;

		LOG_INF("Rainfall: %.2f mm", (double)rainfall);
		LOG_INF("Measurement count: %d", rf->measurement_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_meteo_aggreg(void)
{
	int ret;

	ret = meteo_aggreg_wind_speed();
	if (ret) {
		LOG_ERR("Call `meteo_aggreg_wind_speed` failed: %d", ret);
	}

	ret = meteo_aggreg_wind_direction();
	if (ret) {
		LOG_ERR("Call `meteo_aggreg_wind_direction` failed: %d", ret);
	}

	ret = meteo_aggreg_rainfall();
	if (ret) {
		LOG_ERR("Call `meteo_aggreg_rainfall` failed: %d", ret);
	}

	return 0;
}

int app_sensor_meteo_clear(void)
{
	app_data_lock();

	g_app_data.meteo.wind_speed.measurement_count = 0;
	g_app_data.meteo.wind_direction.measurement_count = 0;
	g_app_data.meteo.rainfall.measurement_count = 0;

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)

int app_sensor_barometer_sample(void)
{
	int ret;

	static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_barometer_tag));

	struct app_data_barometer_pressure *bp = &g_app_data.barometer.pressure;

	if (bp->sample_count < APP_DATA_MAX_SAMPLES) {
		if (!device_is_ready(m_dev)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		ret = sensor_sample_fetch(m_dev);
		if (ret) {
			LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
			return ret;
		}

		struct sensor_value val;
		ret = sensor_channel_get(m_dev, SENSOR_CHAN_PRESS, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			return ret;
		}

		float pressure = sensor_value_to_double(&val);

		LOG_INF("Pressure: %.0f Pa", (double)pressure);

		app_data_lock();

		bp->samples[bp->sample_count] = pressure;
		bp->sample_count++;

		app_data_unlock();

		LOG_INF("Sample count: %d", bp->sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_barometer_aggreg(void)
{
	int ret;

	app_data_lock();

	struct app_data_barometer_pressure *bp = &g_app_data.barometer.pressure;

	if (!bp->measurement_count) {
		ret = ctr_rtc_get_ts(&bp->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (bp->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		aggreg_sample(bp->samples, bp->sample_count,
			      &bp->measurements[bp->measurement_count]);
		bp->measurement_count++;

		LOG_INF("Measurement count: %d", bp->measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	bp->sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_barometer_clear(void)
{
	app_data_lock();

	g_app_data.barometer.pressure.measurement_count = 0;

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)

static void append_hygro_event(enum app_data_hygro_event_type type, float value)
{
	int ret;
	struct app_data_hygro *hygro = &g_app_data.hygro;

	if (hygro->event_count < APP_DATA_MAX_HYGRO_EVENTS) {
		struct app_data_hygro_event *event = &hygro->events[hygro->event_count];

		ret = ctr_rtc_get_ts(&event->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return;
		}

		event->type = type;
		event->value = value;
		hygro->event_count++;

		LOG_INF("Event count: %d", hygro->event_count);
	} else {
		LOG_WRN("Measurement full");
	}
}

static void check_hygro_temperature(float temperature)
{
	struct app_data_hygro *hygro = &g_app_data.hygro;

	bool report_hi_deactivated = false;
	bool report_hi_activated = false;

	float thr_hi = g_app_config.hygro_t_alarm_hi_thr;
	float hst_hi = g_app_config.hygro_t_alarm_hi_hst;

	if (hygro->alarm_hi_active) {
		if (temperature < thr_hi - hst_hi / 2.f) {
			hygro->alarm_hi_active = false;
			report_hi_deactivated = true;
		}
	} else {
		if (temperature > thr_hi + hst_hi / 2.f) {
			hygro->alarm_hi_active = true;
			report_hi_activated = true;
		}
	}

	if (g_app_config.hygro_t_alarm_hi_report && report_hi_deactivated) {
		append_hygro_event(APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_DEACTIVATED, temperature);
		app_work_send_with_rate_limit();
	}

	if (g_app_config.hygro_t_alarm_hi_report && report_hi_activated) {
		append_hygro_event(APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_ACTIVATED, temperature);
		app_work_send_with_rate_limit();
	}

	bool report_lo_deactivated = false;
	bool report_lo_activated = false;

	float thr_lo = g_app_config.hygro_t_alarm_lo_thr;
	float hst_lo = g_app_config.hygro_t_alarm_lo_hst;

	if (hygro->alarm_lo_active) {
		if (temperature > thr_lo + hst_lo / 2.f) {
			hygro->alarm_lo_active = false;
			report_lo_deactivated = true;
		}
	} else {
		if (temperature < thr_lo - hst_lo / 2.f) {
			hygro->alarm_lo_active = true;
			report_lo_activated = true;
		}
	}

	if (g_app_config.hygro_t_alarm_lo_report && report_lo_deactivated) {
		append_hygro_event(APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_DEACTIVATED, temperature);
		app_work_send_with_rate_limit();
	}

	if (g_app_config.hygro_t_alarm_lo_report && report_lo_activated) {
		append_hygro_event(APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_ACTIVATED, temperature);
		app_work_send_with_rate_limit();
	}
}

int app_sensor_hygro_sample(void)
{
	int ret;

	float temperature;
	float humidity;

	if (g_app_data.hygro.sample_count < APP_DATA_MAX_SAMPLES) {
		ret = ctr_hygro_read(&temperature, &humidity);
		if (ret) {
			LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
			temperature = NAN;
			humidity = NAN;
		} else {
			LOG_INF("Hygro: Temperature: %.2f °C", (double)temperature);
			LOG_INF("Hygro: Humidity: %.2f %% RH", (double)humidity);
		}

		app_data_lock();
		g_app_data.hygro.last_sample_temperature = temperature;
		g_app_data.hygro.last_sample_humidity = humidity;
		g_app_data.hygro.samples_temperature[g_app_data.hygro.sample_count] = temperature;
		g_app_data.hygro.samples_humidity[g_app_data.hygro.sample_count] = humidity;
		g_app_data.hygro.sample_count++;
		check_hygro_temperature(temperature);
		app_data_unlock();

		LOG_INF("Sample count: %d", g_app_data.hygro.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

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

	if (g_app_data.hygro.measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		struct app_data_hygro_measurement *hygro_measurement =
			&g_app_data.hygro.measurements[g_app_data.hygro.measurement_count];

		aggreg_sample(g_app_data.hygro.samples_temperature, g_app_data.hygro.sample_count,
			      &hygro_measurement->temperature);

		aggreg_sample(g_app_data.hygro.samples_humidity, g_app_data.hygro.sample_count,
			      &hygro_measurement->humidity);

		g_app_data.hygro.measurement_count++;

		LOG_INF("Measurement count: %d", g_app_data.hygro.measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	g_app_data.hygro.sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_hygro_clear(void)
{
	app_data_lock();
	g_app_data.hygro.measurement_count = 0;
	g_app_data.hygro.event_count = 0;
	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)

int app_sensor_w1_therm_sample(void)
{
	int ret;

	uint64_t serial_number;

	for (int i = 0; i < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		if (g_app_data.w1_therm.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			float temperature;
			ret = ctr_ds18b20_read(i, &serial_number, &temperature);
			if (ret) {
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				continue;
			}

			LOG_INF("Temperature: %.1f C", (double)temperature);

			app_data_lock();

			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];

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

		if (w1_therm->sensor[i].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
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

#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)

int app_sensor_soil_sensor_sample(void)
{
	int ret;

	for (int i = 0; i < MIN(APP_DATA_SOIL_SENSOR_COUNT, ctr_soil_sensor_get_count()); i++) {
		if (g_app_data.soil_sensor.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			uint64_t serial_number;
			float temperature;
			int moisture;
			ret = ctr_soil_sensor_read(i, &serial_number, &temperature, &moisture);
			if (ret) {
				LOG_ERR("Call `ctr_soil_sensor_read` failed: %d", ret);
				continue;
			}

			LOG_INF("Temperature: %.1f C Moisture: %d", (double)temperature, moisture);

			app_data_lock();

			struct app_data_soil_sensor_sensor *sensor =
				&g_app_data.soil_sensor.sensor[i];

			sensor->samples_temperature[sensor->sample_count] = temperature;
			sensor->samples_moisture[sensor->sample_count] = moisture;
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

int app_sensor_soil_sensor_aggreg(void)
{
	int ret;

	struct app_data_soil_sensor *soil_sensor = &g_app_data.soil_sensor;

	app_data_lock();

	for (int i = 0; i < MIN(APP_DATA_SOIL_SENSOR_COUNT, ctr_soil_sensor_get_count()); i++) {
		if (soil_sensor->sensor[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&soil_sensor->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}

		if (soil_sensor->sensor[i].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
			struct app_data_soil_sensor_measurement *measurement =
				&soil_sensor->sensor[i]
					 .measurements[soil_sensor->sensor[i].measurement_count];

			aggreg_sample(soil_sensor->sensor[i].samples_temperature,
				      soil_sensor->sensor[i].sample_count,
				      &measurement->temperature);

			aggreg_sample(soil_sensor->sensor[i].samples_moisture,
				      soil_sensor->sensor[i].sample_count, &measurement->moisture);

			soil_sensor->sensor[i].measurement_count++;

			LOG_INF("Measurement count: %d", soil_sensor->sensor[i].measurement_count);

		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		soil_sensor->sensor[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_soil_sensor_clear(void)
{
	app_data_lock();

	for (int i = 0; i < MIN(APP_DATA_SOIL_SENSOR_COUNT, ctr_soil_sensor_get_count()); i++) {
		g_app_data.soil_sensor.sensor[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

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
			if (ret && ret != -ENOENT) {
				LOG_ERR("Call `ctr_ble_tag_read_cached` failed: %d", ret);
				continue;
			} else if (ret == -ENOENT) {
				continue;
			}

			app_data_lock();
			memcpy(sensor->addr, addr, BT_ADDR_SIZE);

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_RSSI) {
				sensor->rssi = rssi;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
				sensor->voltage = voltage;
			}

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

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)

static int lambrecht_sample_wind_speed(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_wind_speed(
		&lambrecht->wind_speed_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_wind_speed` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Wind speed: %.1f m/s", lambrecht->wind_speed_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_wind_direction(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_wind_direction(
		&lambrecht->wind_direction_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_wind_direction` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Wind direction: %.1f deg",
		lambrecht->wind_direction_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_temperature(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_temperature(
		&lambrecht->temperature_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_temperature` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Temperature: %.1f C", lambrecht->temperature_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_humidity(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_humidity(&lambrecht->humidity_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_humidity` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Humidity: %.1f %%", lambrecht->humidity_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_dew_point(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_dew_point(&lambrecht->dew_point_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_dew_point` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Dew point: %.1f C", lambrecht->dew_point_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_pressure(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_pressure(&lambrecht->pressure_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_pressure` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Pressure: %.1f kPa", lambrecht->pressure_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_rainfall_total(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_rainfall_total(
		&lambrecht->rainfall_total_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_rainfall_total` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Rainfall total: %.1f mm",
		lambrecht->rainfall_total_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_rainfall_intensity(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_rainfall_intensity(
		&lambrecht->rainfall_intensity_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_rainfall_intensity` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Rainfall total: %.3f mm/s",
		lambrecht->rainfall_intensity_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

static int lambrecht_sample_illuminance(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	ret = app_lambrecht_read_illuminance(
		&lambrecht->illuminance_samples[lambrecht->sample_count]);
	if (ret) {
		LOG_ERR("Call `app_lambrecht_read_illuminance` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	LOG_INF("Illuminance: %.3f klx", lambrecht->illuminance_samples[lambrecht->sample_count]);

	app_data_unlock();

	return 0;
}

int app_sensor_lambrecht_sample(void)
{
	int ret;

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	ret = app_lambrecht_enable();
	if (ret) {
		LOG_ERR("Call `lambrecht_enable` failed: %d", ret);
	}

	ret = lambrecht_sample_wind_speed();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_wind_speed` failed: %d", ret);
	}

	ret = lambrecht_sample_wind_direction();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_wind_direction` failed: %d", ret);
	}

	ret = lambrecht_sample_temperature();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_temperature` failed: %d", ret);
	}

	ret = lambrecht_sample_humidity();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_humidity` failed: %d", ret);
	}

	ret = lambrecht_sample_dew_point();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_dew_point` failed: %d", ret);
	}

	ret = lambrecht_sample_pressure();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_pressure` failed: %d", ret);
	}

	ret = lambrecht_sample_rainfall_total();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_rainfall_total` failed: %d", ret);
	}

	ret = lambrecht_sample_rainfall_intensity();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_rainfall_intensity` failed: %d", ret);
	}

	ret = lambrecht_sample_illuminance();
	if (ret) {
		LOG_ERR("Call `lambrecht_sample_illuminance` failed: %d", ret);
	}

	lambrecht->sample_count++;

	ret = app_lambrecht_disable();
	if (ret) {
		LOG_ERR("Call `lambrecht_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_sensor_lambrecht_aggreg(void)
{
	int ret;

	app_data_lock();

	struct app_data_lambrecht *lambrecht = &g_app_data.lambrecht;

	if (lambrecht->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		ret = ctr_rtc_get_ts(&lambrecht->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc-get_tc` failed: %d", ret);
			app_data_unlock();
			return ret;
		}

		aggreg_sample(lambrecht->wind_speed_samples, lambrecht->sample_count,
			      &lambrecht->wind_speed_measurements[lambrecht->measurement_count]);
		aggreg_sample(
			lambrecht->wind_direction_samples, lambrecht->sample_count,
			&lambrecht->wind_direction_measurements[lambrecht->measurement_count]);
		aggreg_sample(lambrecht->temperature_samples, lambrecht->sample_count,
			      &lambrecht->temperature_measurements[lambrecht->measurement_count]);
		aggreg_sample(lambrecht->humidity_samples, lambrecht->sample_count,
			      &lambrecht->humidity_measurements[lambrecht->measurement_count]);
		aggreg_sample(lambrecht->dew_point_samples, lambrecht->sample_count,
			      &lambrecht->dew_point_measurements[lambrecht->measurement_count]);
		aggreg_sample(lambrecht->pressure_samples, lambrecht->sample_count,
			      &lambrecht->pressure_measurements[lambrecht->measurement_count]);
		aggreg_sample(
			lambrecht->rainfall_total_samples, lambrecht->sample_count,
			&lambrecht->rainfall_total_measurements[lambrecht->measurement_count]);
		aggreg_sample(
			lambrecht->rainfall_intensity_samples, lambrecht->sample_count,
			&lambrecht->rainfall_intensity_measurements[lambrecht->measurement_count]);
		aggreg_sample(lambrecht->illuminance_samples, lambrecht->sample_count,
			      &lambrecht->illuminance_measurements[lambrecht->measurement_count]);

		for (int i = 0; i < lambrecht->sample_count; i++) {
			lambrecht->rainfall_total_sum += lambrecht->rainfall_total_samples[i];
		}

		lambrecht->sample_count = 0;
		lambrecht->measurement_count++;

	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_lambrecht_clear(void)
{
	app_data_lock();

	g_app_data.lambrecht.measurement_count = 0;
	g_app_data.lambrecht.rainfall_total_sum = 0;

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */

#if defined(CONFIG_APP_PYRANOMETER)

int app_sensor_pyranometer_sample(void)
{
	int ret;

	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 1);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(100));

	struct app_data_pyranometer *pyranometer = &g_app_data.pyranometer;

	app_data_lock();

	if (pyranometer->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 0);
		return -ENOSPC;
	}

	uint16_t sample_1;
	ret = ctr_adc_read(CTR_ADC_CHANNEL_B0, &sample_1);
	if (ret) {
		LOG_ERR("Call `ctr_adc_read` failed (1): %d", ret);
		app_data_unlock();
		ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 0);
		return ret;
	}

	pyranometer->irradiance_1_samples[pyranometer->sample_count] = CTR_ADC_MILLIVOLTS(sample_1);

	LOG_INF("Irradiance 1 response: %.0f mV",
		pyranometer->irradiance_1_samples[pyranometer->sample_count]);

	uint16_t sample_2;
	ret = ctr_adc_read(CTR_ADC_CHANNEL_B1, &sample_2);
	if (ret) {
		LOG_ERR("Call `ctr_adc_read` failed (2): %d", ret);
		app_data_unlock();
		ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 0);
		return ret;
	}

	pyranometer->irradiance_2_samples[pyranometer->sample_count] = CTR_ADC_MILLIVOLTS(sample_2);

	LOG_INF("Irradiance 2 response: %.0f mV",
		pyranometer->irradiance_2_samples[pyranometer->sample_count]);

	pyranometer->sample_count++;

	app_data_unlock();

	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 0);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_sensor_pyranometer_aggreg(void)
{
	int ret;

	app_data_lock();

	struct app_data_pyranometer *pyranometer = &g_app_data.pyranometer;

	if (pyranometer->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		ret = ctr_rtc_get_ts(&pyranometer->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc-get_tc` failed: %d", ret);
			app_data_unlock();
			return ret;
		}

		aggreg_sample(
			pyranometer->irradiance_1_samples, pyranometer->sample_count,
			&pyranometer->irradiance_1_measurements[pyranometer->measurement_count]);

		aggreg_sample(
			pyranometer->irradiance_2_samples, pyranometer->sample_count,
			&pyranometer->irradiance_2_measurements[pyranometer->measurement_count]);

		pyranometer->sample_count = 0;
		pyranometer->measurement_count++;

	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_pyranometer_clear(void)
{
	app_data_lock();

	g_app_data.pyranometer.measurement_count = 0;

	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_APP_PYRANOMETER) */
