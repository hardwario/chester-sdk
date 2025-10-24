/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_sensor.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_radon.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_rtd.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_sps30.h>
#include <chester/ctr_tc.h>
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
		avg_ += (double)samples[i];
	}
	avg_ /= count;

	*avg = avg_;
	*mdn = samples[count / 2];
}

__unused static void aggreg_sample(float *samples, size_t count, struct ctr_data_aggreg *sample)
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

#if defined(FEATURE_HARDWARE_CHESTER_S1)

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

	struct app_data_iaq_sensors *sensors = &g_app_data.iaq.sensors;

	if (sensors->sample_count < APP_DATA_MAX_SAMPLES) {
		int i = sensors->sample_count;
		ret = ctr_s1_read_temperature(dev, &temperature);
		if (ret) {
			temperature = NAN;
			LOG_ERR("Call `ctr_s1_read_temperature` failed: %d", ret);
		} else {
			LOG_INF("Temperature: %.1f C", (double)temperature);
		}

		ret = ctr_s1_read_humidity(dev, &humidity);
		if (ret) {
			humidity = NAN;
			LOG_ERR("Call `ctr_s1_read_humidity` failed: %d", ret);
		} else {
			LOG_INF("Humidity: %.1f %%", (double)humidity);
		}

		ret = ctr_s1_read_illuminance(dev, &illuminance);
		if (ret) {
			illuminance = NAN;
			LOG_ERR("Call `ctr_s1_read_illuminance` failed: %d", ret);
		} else {
			LOG_INF("Illuminance: %.0f lux", (double)illuminance);
		}

		ret = ctr_s1_read_altitude(dev, &altitude);
		if (ret) {
			altitude = NAN;
			LOG_ERR("Call `ctr_s1_read_altitude` failed: %d", ret);
		} else {
			LOG_INF("Altitude: %.0f m", (double)altitude);
		}

		ret = ctr_s1_read_pressure(dev, &pressure);
		if (ret) {
			pressure = NAN;
			LOG_ERR("Call `ctr_s1_read_pressure` failed: %d", ret);
		} else {
			LOG_INF("Pressure: %.0f Pa", (double)pressure);
		}

		ret = ctr_s1_read_co2_conc(dev, &co2_conc);
		if (ret) {
			co2_conc = NAN;
			LOG_ERR("Call `ctr_s1_read_co2_conc` failed: %d", ret);
		} else {
			LOG_INF("CO2 conc.: %.0f ppm", (double)co2_conc);
		}

		app_data_lock();
		sensors->samples_temperature[i] = temperature;
		sensors->samples_humidity[i] = humidity;
		sensors->samples_illuminance[i] = illuminance;
		sensors->samples_altitude[i] = altitude;
		sensors->samples_pressure[i] = pressure;
		sensors->samples_co2_conc[i] = co2_conc;
		sensors->sample_count++;

		sensors->last_co2_conc = co2_conc;
		sensors->last_temperature = temperature;
		sensors->last_humidity = humidity;
		sensors->last_illuminance = illuminance;
		sensors->last_altitude = altitude;
		sensors->last_pressure = pressure;
		app_data_unlock();

		LOG_INF("Sample count: %d", sensors->sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_iaq_sensors_aggreg(void)
{
	int ret;
	struct app_data_iaq_sensors *sensors = &g_app_data.iaq.sensors;

	app_data_lock();

	if (sensors->measurement_count == 0) {
		ret = ctr_rtc_get_ts(&sensors->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (sensors->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		struct app_data_iaq_sensors_measurement *measurement =
			&sensors->measurements[sensors->measurement_count];

		aggreg_sample(sensors->samples_temperature, sensors->sample_count,
			      &measurement->temperature);
		aggreg_sample(sensors->samples_humidity, sensors->sample_count,
			      &measurement->humidity);
		aggreg_sample(sensors->samples_illuminance, sensors->sample_count,
			      &measurement->illuminance);
		aggreg_sample(sensors->samples_altitude, sensors->sample_count,
			      &measurement->altitude);
		aggreg_sample(sensors->samples_pressure, sensors->sample_count,
			      &measurement->pressure);
		aggreg_sample(sensors->samples_co2_conc, sensors->sample_count,
			      &measurement->co2_conc);

		measurement->motion_count = sensors->motion_count;

		sensors->measurement_count++;

		LOG_WRN("Measurement count: %d", sensors->measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	sensors->sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_iaq_button_aggreg(void)
{
	int ret;
	struct app_data_iaq_button *button = &g_app_data.iaq.button;

	app_data_lock();

	if (button->measurement_count == 0) {
		ret = ctr_rtc_get_ts(&button->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (button->measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		struct app_data_iaq_button_measurement *measurement =
			&button->measurements[button->measurement_count];

		measurement->press_count = button->press_count;

		button->measurement_count++;

		LOG_WRN("Measurement count: %d", button->measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_iaq_clear(void)
{
	app_data_lock();
	g_app_data.iaq.sensors.measurement_count = 0;
	g_app_data.iaq.button.measurement_count = 0;
	app_data_unlock();

	return 0;
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

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
			float temperature = NAN;
			ret = ctr_ds18b20_read(i, &serial_number, &temperature);

			if (ret) {
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", (double)temperature);
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

#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B)

int app_sensor_rtd_therm_sample(void)
{
	int ret;
	const enum ctr_rtd_channel channel_lookup[] = {
#if defined(FEATURE_HARDWARE_CHESTER_RTD_A)
		CTR_RTD_CHANNEL_A1,
		CTR_RTD_CHANNEL_A2,
#endif
#if defined(FEATURE_HARDWARE_CHESTER_RTD_B)
		CTR_RTD_CHANNEL_B1,
		CTR_RTD_CHANNEL_B2,
#endif
	};

	for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
		if (g_app_data.rtd_therm.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			float temperature = NAN;
			ret = ctr_rtd_read(channel_lookup[i], CTR_RTD_TYPE_PT1000, &temperature);

			if (ret) {
				LOG_WRN("Call `ctr_rtd_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", (double)temperature);
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

#endif /* defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B) */

#if defined(FEATURE_HARDWARE_CHESTER_TC_A) || defined(FEATURE_HARDWARE_CHESTER_TC_B)

int app_sensor_tc_therm_sample(void)
{
	int ret;
	const enum ctr_tc_channel channel_lookup[] = {
#if defined(FEATURE_HARDWARE_CHESTER_TC_A)
		CTR_TC_CHANNEL_A1,
		CTR_TC_CHANNEL_A2,
#endif
#if defined(FEATURE_HARDWARE_CHESTER_TC_B)
		CTR_TC_CHANNEL_B1,
		CTR_TC_CHANNEL_B2,
#endif
	};

	for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {
		if (g_app_data.tc_therm.sensor[i].sample_count < APP_DATA_MAX_SAMPLES) {
			float temperature = NAN;
			ret = ctr_tc_read(channel_lookup[i], CTR_TC_TYPE_K, &temperature);

			if (ret) {
				LOG_WRN("Call `ctr_tc_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C", (double)temperature);
			}

			app_data_lock();
			struct app_data_tc_therm_sensor *sensor = &g_app_data.tc_therm.sensor[i];
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

int app_sensor_tc_therm_aggreg(void)
{
	int ret;

	struct app_data_tc_therm *tc_therm = &g_app_data.tc_therm;

	app_data_lock();

	for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {
		if (tc_therm->sensor[i].measurement_count == 0) {
			ret = ctr_rtc_get_ts(&tc_therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				app_data_unlock();
				return ret;
			}
		}
		if (tc_therm->sensor[i].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
			struct app_data_tc_therm_measurement *measurement =
				&tc_therm->sensor[i]
					 .measurements[tc_therm->sensor[i].measurement_count];

			aggreg_sample(tc_therm->sensor[i].samples_temperature,
				      tc_therm->sensor[i].sample_count, &measurement->temperature);

			tc_therm->sensor[i].measurement_count++;

			LOG_INF("Measurement count: %d", tc_therm->sensor[i].measurement_count);
		} else {
			LOG_WRN("Measurement buffer full");
			app_data_unlock();
			return -ENOSPC;
		}

		tc_therm->sensor[i].sample_count = 0;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_tc_therm_clear(void)
{
	app_data_lock();

	for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {
		g_app_data.tc_therm.sensor[i].measurement_count = 0;
	}

	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_TC_A) || defined(FEATURE_HARDWARE_CHESTER_TC_B)         \
	*/

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

			sensor->last_temperature = temperature;
			sensor->last_moisture = moisture;
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

#if defined(FEATURE_HARDWARE_CHESTER_SPS30)

int app_sensor_sps30_sample(void)
{
	int ret;

	float pm_1_0;
	float pm_2_5;
	float pm_4_0;
	float pm_10_0;

	float num_0_5;
	float num_1_0;
	float num_2_5;
	float num_4_0;
	float num_10_0;

	if (g_app_data.sps30.sample_count < APP_DATA_MAX_SAMPLES) {
		ret = ctr_sps30_read(&pm_1_0, &pm_2_5, &pm_4_0, &pm_10_0, &num_0_5, &num_1_0,
				     &num_2_5, &num_4_0, &num_10_0);
		if (ret) {
			LOG_ERR("Call `ctr_sps30_read` failed: %d", ret);
			pm_1_0 = NAN;
			pm_2_5 = NAN;
			pm_4_0 = NAN;
			pm_10_0 = NAN;

			num_0_5 = NAN;
			num_1_0 = NAN;
			num_2_5 = NAN;
			num_4_0 = NAN;
			num_10_0 = NAN;
		} else {
			LOG_INF("SPS30: Mass Concentration PM1.0: %.2f ug/m3", (double)pm_1_0);
			LOG_INF("SPS30: Mass Concentration PM2.5: %.2f ug/m3", (double)pm_2_5);
			LOG_INF("SPS30: Mass Concentration PM4.0: %.2f ug/m3", (double)pm_4_0);
			LOG_INF("SPS30: Mass Concentration PM10.0: %.2f ug/m3", (double)pm_10_0);

			LOG_INF("SPS30: Number Concentration PM0.5: %.2f", (double)num_0_5);
			LOG_INF("SPS30: Number Concentration PM1.0: %.2f", (double)num_1_0);
			LOG_INF("SPS30: Number Concentration PM2.5: %.2f", (double)num_2_5);
			LOG_INF("SPS30: Number Concentration PM4.0: %.2f", (double)num_4_0);
			LOG_INF("SPS30: Number Concentration PM10.0: %.2f", (double)num_10_0);
		}

		app_data_lock();
		g_app_data.sps30.last_sample_mass_conc_pm_1_0 = pm_1_0;
		g_app_data.sps30.last_sample_mass_conc_pm_2_5 = pm_2_5;
		g_app_data.sps30.last_sample_mass_conc_pm_4_0 = pm_4_0;
		g_app_data.sps30.last_sample_mass_conc_pm_10_0 = pm_10_0;

		g_app_data.sps30.last_sample_num_conc_pm_0_5 = num_0_5;
		g_app_data.sps30.last_sample_num_conc_pm_1_0 = num_1_0;
		g_app_data.sps30.last_sample_num_conc_pm_2_5 = num_2_5;
		g_app_data.sps30.last_sample_num_conc_pm_4_0 = num_4_0;
		g_app_data.sps30.last_sample_num_conc_pm_10_0 = num_10_0;

		g_app_data.sps30.samples_mass_conc_pm_1_0[g_app_data.sps30.sample_count] = pm_1_0;
		g_app_data.sps30.samples_mass_conc_pm_2_5[g_app_data.sps30.sample_count] = pm_2_5;
		g_app_data.sps30.samples_mass_conc_pm_4_0[g_app_data.sps30.sample_count] = pm_4_0;
		g_app_data.sps30.samples_mass_conc_pm_10_0[g_app_data.sps30.sample_count] = pm_10_0;

		g_app_data.sps30.samples_num_conc_pm_0_5[g_app_data.sps30.sample_count] = num_0_5;
		g_app_data.sps30.samples_num_conc_pm_1_0[g_app_data.sps30.sample_count] = num_1_0;
		g_app_data.sps30.samples_num_conc_pm_2_5[g_app_data.sps30.sample_count] = num_2_5;
		g_app_data.sps30.samples_num_conc_pm_4_0[g_app_data.sps30.sample_count] = num_4_0;
		g_app_data.sps30.samples_num_conc_pm_10_0[g_app_data.sps30.sample_count] = num_10_0;

		g_app_data.sps30.sample_count++;
		app_data_unlock();

		LOG_INF("Sample count: %d", g_app_data.sps30.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		return -ENOSPC;
	}

	return 0;
}

int app_sensor_sps30_aggreg(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.sps30.measurement_count == 0) {
		ret = ctr_rtc_get_ts(&g_app_data.sps30.timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}
	}

	if (g_app_data.sps30.measurement_count < APP_DATA_MAX_MEASUREMENTS) {
		struct app_data_sps30_measurement *sps30_measurement =
			&g_app_data.sps30.measurements[g_app_data.sps30.measurement_count];

		aggreg_sample(g_app_data.sps30.samples_mass_conc_pm_1_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->mass_conc_pm_1_0);
		aggreg_sample(g_app_data.sps30.samples_mass_conc_pm_2_5,
			      g_app_data.sps30.sample_count, &sps30_measurement->mass_conc_pm_2_5);
		aggreg_sample(g_app_data.sps30.samples_mass_conc_pm_4_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->mass_conc_pm_4_0);
		aggreg_sample(g_app_data.sps30.samples_mass_conc_pm_10_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->mass_conc_pm_10_0);

		aggreg_sample(g_app_data.sps30.samples_num_conc_pm_0_5,
			      g_app_data.sps30.sample_count, &sps30_measurement->num_conc_pm_0_5);
		aggreg_sample(g_app_data.sps30.samples_num_conc_pm_1_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->num_conc_pm_1_0);
		aggreg_sample(g_app_data.sps30.samples_num_conc_pm_2_5,
			      g_app_data.sps30.sample_count, &sps30_measurement->num_conc_pm_2_5);
		aggreg_sample(g_app_data.sps30.samples_num_conc_pm_4_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->num_conc_pm_4_0);
		aggreg_sample(g_app_data.sps30.samples_num_conc_pm_10_0,
			      g_app_data.sps30.sample_count, &sps30_measurement->num_conc_pm_10_0);

		g_app_data.sps30.measurement_count++;

		LOG_INF("Measurement count: %d", g_app_data.sps30.measurement_count);
	} else {
		LOG_WRN("Measurement buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	g_app_data.sps30.sample_count = 0;

	app_data_unlock();

	return 0;
}

int app_sensor_sps30_clear(void)
{
	app_data_lock();
	g_app_data.sps30.measurement_count = 0;
	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_SPS30) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
int app_sensor_ble_tag_sample(void)
{
	int ret;

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		struct app_data_ble_tag_sensor *sensor = &g_app_data.ble_tag.sensor[i];
		sensor->voltage = NAN;

		if (g_app_data.ble_tag.sensor[i].sample_count < APP_DATA_MAX_BLE_TAG_SAMPLES) {
			uint8_t addr[BT_ADDR_SIZE] = {0};
			int8_t rssi = 0;
			float voltage = NAN;
			float temperature = NAN;
			float humidity = NAN;
			int16_t sensor_mask = 0;
			bool valid = false;

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

			memcpy(sensor->addr, addr, BT_ADDR_SIZE);

			if (valid) {
				sensor->last_sample_temperature = temperature;
				sensor->last_sample_humidity = humidity;
				sensor->rssi = rssi;
				sensor->voltage = voltage;

				sensor->samples_temperature[sensor->sample_count] = temperature;
				sensor->samples_humidity[sensor->sample_count] = humidity;
				sensor->sample_count++;

				LOG_INF("Sensor %d: sample count: %d", i, sensor->sample_count);
			} else {
				sensor->last_sample_temperature = NAN;
				sensor->last_sample_humidity = NAN;
				sensor->rssi = 0;
				sensor->voltage = NAN;
				LOG_INF("Sensor %d: no valid data", i);
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

#if defined(FEATURE_SUBSYSTEM_RADON)
int app_sensor_radon_sample(void)
{
	int ret;

	if (g_app_data.radon.sample_count < APP_DATA_MAX_SAMPLES) {
		ctr_radon_enable();

		uint16_t hum;
		ret = ctr_radon_read_humidity(&hum);
		if (ret) {
			LOG_ERR("Call `app_radon_read_humidity` failed: %d", ret);
			return ret;
		}

		LOG_INF("Chamber humidity: %d %%", hum);

		uint16_t temp;
		ret = ctr_radon_read_temperature(&temp);
		if (ret) {
			LOG_ERR("Call `app_radon_read_temperature` failed: %d", ret);
			return ret;
		}

		ctr_radon_disable();

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

		ctr_radon_enable();

		ret = ctr_radon_read_concentration(
			&radon->concentration_measurements[radon->measurement_count], false);
		if (ret) {
			LOG_ERR("Call `app_radon_read_concentration` failed: %d", ret);
			return ret;
		}

		ret = ctr_radon_read_concentration(
			&radon->daily_concentration_measurements[radon->measurement_count], true);
		if (ret) {
			LOG_ERR("Call `app_radon_read_concentration` failed: %d", ret);
			return ret;
		}

		ctr_radon_disable();

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
#endif /* defined(FEATURE_SUBSYSTEM_RADON) */
