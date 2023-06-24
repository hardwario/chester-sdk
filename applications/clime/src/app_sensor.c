/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_rtd.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_batt.h>
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
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

#define BATT_TEST_INTERVAL_MSEC (12 * 60 * 60 * 1000)

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

#if defined(CONFIG_SHIELD_CTR_S1)

int app_sensor_iaq_sample(void)
{
	int ret;
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

	float temperature;
	float humidity;
	float illuminance;
	float altitude;
	float pressure;
	float co2_conc;

	if (g_app_data.iaq.sample_count < APP_DATA_MAX_SAMPLES) {
		int i = g_app_data.iaq.sample_count;
		ret = ctr_s1_read_temperature(dev, &temperature);
		if (ret) {
			temperature = NAN;
			LOG_ERR("Call `ctr_s1_read_temperature` failed: %d", ret);
		} else {
			LOG_INF("Temperature: %.1f C", temperature);
		}

		ret = ctr_s1_read_humidity(dev, &humidity);
		if (ret) {
			humidity = NAN;
			LOG_ERR("Call `ctr_s1_read_humidity` failed: %d", ret);
		} else {
			LOG_INF("Humidity: %.1f %%", humidity);
		}

		ret = ctr_s1_read_illuminance(dev, &illuminance);
		if (ret) {
			illuminance = NAN;
			LOG_ERR("Call `ctr_s1_read_illuminance` failed: %d", ret);
		} else {
			LOG_INF("Illuminance: %.0f lux", illuminance);
		}

		ret = ctr_s1_read_altitude(dev, &altitude);
		if (ret) {
			altitude = NAN;
			LOG_ERR("Call `ctr_s1_read_altitude` failed: %d", ret);
		} else {
			LOG_INF("Altitude: %.0f m", altitude);
		}

		ret = ctr_s1_read_pressure(dev, &pressure);
		if (ret) {
			pressure = NAN;
			LOG_ERR("Call `ctr_s1_read_pressure` failed: %d", ret);
		} else {
			LOG_INF("Pressure: %.0f Pa", pressure);
		}

		ret = ctr_s1_read_co2_conc(dev, &co2_conc);
		if (ret) {
			co2_conc = NAN;
			LOG_ERR("Call `ctr_s1_read_co2_conc` failed: %d", ret);
		} else {
			LOG_INF("CO2 conc.: %.0f ppm", co2_conc);
		}

		app_data_lock();
		g_app_data.iaq.samples_temperature[i] = temperature;
		g_app_data.iaq.samples_humidity[i] = humidity;
		g_app_data.iaq.samples_illuminance[i] = illuminance;
		g_app_data.iaq.samples_altitude[i] = altitude;
		g_app_data.iaq.samples_pressure[i] = pressure;
		g_app_data.iaq.samples_co2_conc[i] = co2_conc;
		g_app_data.iaq.sample_count++;
		app_data_unlock();

		LOG_INF("Sample count: %d", g_app_data.iaq.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_iaq_aggreg(void)
{
	int ret;
	struct app_data_iaq *iaq = &g_app_data.iaq;

	app_data_lock();

	if (iaq->measurement_count == 0) {
		ret = ctr_rtc_get_ts(&iaq->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (iaq->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		struct app_data_iaq_measurement *measurement =
			&iaq->measurements[iaq->measurement_count];

		aggreg_sample(iaq->samples_temperature, iaq->sample_count,
			      &measurement->temperature);
		aggreg_sample(iaq->samples_humidity, iaq->sample_count, &measurement->humidity);
		aggreg_sample(iaq->samples_illuminance, iaq->sample_count,
			      &measurement->illuminance);
		aggreg_sample(iaq->samples_altitude, iaq->sample_count, &measurement->altitude);
		aggreg_sample(iaq->samples_pressure, iaq->sample_count, &measurement->pressure);
		aggreg_sample(iaq->samples_co2_conc, iaq->sample_count, &measurement->co2_conc);

		measurement->press_count = iaq->press_count;
		measurement->motion_count = iaq->motion_count;

		iaq->measurement_count++;

		LOG_WRN("Measurement count: %d", iaq->measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	iaq->sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_iaq_clear(void)
{
	app_data_lock();
	g_app_data.iaq.measurement_count = 0;
	app_data_unlock();

	return 0;
}
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)

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
			LOG_INF("Hygro: Temperature: %.2f °C", temperature);
			LOG_INF("Hygro: Humidity: %.2f %% RH", humidity);
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

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

int app_sensor_w1_therm_sample(void)
{
	int ret;
	uint64_t serial_number;

	for (int i = 0; i < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		if (g_app_data.w1_therm.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
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

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)

int app_sensor_rtd_therm_sample(void)
{
	int ret;
	const enum ctr_rtd_channel channel_lookup[] = {
#if defined(CONFIG_SHIELD_CTR_RTD_A)
		CTR_RTD_CHANNEL_A1,
		CTR_RTD_CHANNEL_A2,
#endif
#if defined(CONFIG_SHIELD_CTR_RTD_B)
		CTR_RTD_CHANNEL_B1,
		CTR_RTD_CHANNEL_B2,
#endif
	};

	for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
		if (g_app_data.rtd_therm.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			float temperature = NAN;
			ret = ctr_rtd_read(channel_lookup[i], CTR_RTD_TYPE_PT1000, &temperature);

			if (ret) {
				LOG_ERR("Call `ctr_rtd_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", temperature);
			}

			app_data_lock();
			struct app_data_rtd_therm_sensor *sensor = &g_app_data.rtd_therm.sensor[i];
			sensor->last_sample_temperature = temperature;
			sensor->samples_temperature[sensor->sample_count] = temperature;
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

int app_sensor_rtd_therm_aggreg(void)
{
	int ret;

	struct app_data_rtd_therm *rtd_therm = &g_app_data.rtd_therm;

	app_data_lock();

	for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
		if (rtd_therm->sensor[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&rtd_therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}
		if (rtd_therm->sensor[i].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
			struct app_data_rtd_therm_measurement *measurement =
				&rtd_therm->sensor[i]
					 .measurements[rtd_therm->sensor[i].measurement_count];

			aggreg_sample(rtd_therm->sensor[i].samples_temperature,
				      rtd_therm->sensor[i].sample_count, &measurement->temperature);

			rtd_therm->sensor[i].measurement_count++;

			LOG_INF("Measurement count: %d", rtd_therm->sensor[i].measurement_count);
		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		rtd_therm->sensor[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_rtd_therm_clear(void)
{
	app_data_lock();

	for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
		g_app_data.rtd_therm.sensor[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)

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

			LOG_INF("Temperature: %.1f C Moisture: %d", temperature, moisture);

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

#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */
