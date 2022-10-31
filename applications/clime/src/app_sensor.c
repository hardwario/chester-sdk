#include "app_config.h"
#include "app_data.h"
#include "app_send.h"
#include "app_sensor.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_batt.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

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

static void aggregate(float *samples, size_t count, float *minimum, float *maximum, float *average,
                      float *median)
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

/* TODO check if linked */
__unused static void aggregate_sample(float *samples, size_t count, struct app_data_aggreg *sample)
{
	aggregate(samples, count, &sample->min, &sample->max, &sample->avg, &sample->mdn);
}

static int update_battery(void)
{
	int ret;

	static int64_t next;

	if (k_uptime_get() < next) {
		return 0;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto error;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv, CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
		goto error;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(dev, &load_mv, CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
		goto error;
	}

	int current_ma;
	ctr_batt_get_load_current_ma(dev, &current_ma, load_mv);

	LOG_INF("Battery voltage (rest): %u mV", rest_mv);
	LOG_INF("Battery voltage (load): %u mV", load_mv);
	LOG_INF("Battery current (load): %u mA", current_ma);

	app_data_lock();
	g_app_data.batt_voltage_rest = rest_mv / 1000.f;
	g_app_data.batt_voltage_load = load_mv / 1000.f;
	g_app_data.batt_current_load = current_ma;
	app_data_unlock();

	next = k_uptime_get() + BATT_TEST_INTERVAL_MSEC;

	return 0;

error:
	app_data_lock();
	g_app_data.batt_voltage_rest = NAN;
	g_app_data.batt_voltage_load = NAN;
	g_app_data.batt_current_load = NAN;
	app_data_unlock();

	return ret;
}

int app_sensor_sample(void)
{
	int ret;

	app_data_lock();

	ret = update_battery();
	if (ret) {
		LOG_ERR("Call `update_battery` failed: %d", ret);
	}

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

	app_data_unlock();

	return 0;
}

#if defined(CONFIG_SHIELD_CTR_S1)

int app_sensor_iaq_sample(void)
{
	int ret;
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

	app_data_lock();

	if (g_app_data.iaq.sample_count < APP_DATA_MAX_SAMPLES) {
		int i = g_app_data.iaq.sample_count;
		ret = ctr_s1_read_temperature(dev, &g_app_data.iaq.samples_temperature[i]);
		if (ret) {
			g_app_data.iaq.samples_temperature[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_temperature` failed: %d", ret);
		} else {
			LOG_INF("Temperature: %.1f C", g_app_data.iaq.samples_temperature[i]);
		}

		ret = ctr_s1_read_humidity(dev, &g_app_data.iaq.samples_humidity[i]);
		if (ret) {
			g_app_data.iaq.samples_humidity[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_humidity` failed: %d", ret);
		} else {
			LOG_INF("Humidity: %.1f %%", g_app_data.iaq.samples_humidity[i]);
		}

		ret = ctr_s1_read_illuminance(dev, &g_app_data.iaq.samples_illuminance[i]);
		if (ret) {
			g_app_data.iaq.samples_illuminance[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_illuminance` failed: %d", ret);
		} else {
			LOG_INF("Illuminance: %.0f lux", g_app_data.iaq.samples_illuminance[i]);
		}

		ret = ctr_s1_read_altitude(dev, &g_app_data.iaq.samples_altitude[i]);
		if (ret) {
			g_app_data.iaq.samples_altitude[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_altitude` failed: %d", ret);
		} else {
			LOG_INF("Altitude: %.0f m", g_app_data.iaq.samples_altitude[i]);
		}

		ret = ctr_s1_read_pressure(dev, &g_app_data.iaq.samples_pressure[i]);
		if (ret) {
			g_app_data.iaq.samples_pressure[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_pressure` failed: %d", ret);
		} else {
			LOG_INF("Pressure: %.0f Pa", g_app_data.iaq.samples_pressure[i]);
		}

		ret = ctr_s1_read_co2_conc(dev, &g_app_data.iaq.samples_co2_conc[i]);
		if (ret) {
			g_app_data.iaq.samples_co2_conc[i] = NAN;
			LOG_ERR("Call `ctr_s1_read_co2_conc` failed: %d", ret);
		} else {
			LOG_INF("CO2 conc.: %.0f ppm", g_app_data.iaq.samples_co2_conc[i]);
		}

		g_app_data.iaq.sample_count++;

		LOG_INF("Sample count: %d", g_app_data.iaq.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_iaq_aggregate(void)
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

		aggregate_sample(iaq->samples_temperature, iaq->sample_count,
		                 &measurement->temperature);
		aggregate_sample(iaq->samples_humidity, iaq->sample_count, &measurement->humidity);
		aggregate_sample(iaq->samples_illuminance, iaq->sample_count,
		                 &measurement->illuminance);
		aggregate_sample(iaq->samples_altitude, iaq->sample_count, &measurement->altitude);
		aggregate_sample(iaq->samples_pressure, iaq->sample_count, &measurement->pressure);
		aggregate_sample(iaq->samples_co2_conc, iaq->sample_count, &measurement->co2_conc);

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

int app_sensor_hygro_sample(void)
{
	int ret;

	app_data_lock();

	if (g_app_data.hygro.sample_count < APP_DATA_MAX_SAMPLES) {
		ret = ctr_hygro_read(
		        &g_app_data.hygro.samples_temperature[g_app_data.hygro.sample_count],
		        &g_app_data.hygro.samples_humidity[g_app_data.hygro.sample_count]);
		if (ret) {
			LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
			g_app_data.hygro.samples_temperature[g_app_data.hygro.sample_count] = NAN;
			g_app_data.hygro.samples_humidity[g_app_data.hygro.sample_count] = NAN;
		} else {
			LOG_INF("Temperature: %.2f °C",
			        g_app_data.hygro
			                .samples_temperature[g_app_data.hygro.sample_count]);
			LOG_INF("Humidity: %.2f %% RH",
			        g_app_data.hygro.samples_humidity[g_app_data.hygro.sample_count]);
		}

		g_app_data.hygro.sample_count++;

		LOG_INF("Sample count: %d", g_app_data.hygro.sample_count);
	} else {
		LOG_WRN("Sample buffer full");
		app_data_unlock();
		return -ENOSPC;
	}

	app_data_unlock();

	return 0;
}

int app_sensor_hygro_aggregate(void)
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

		aggregate_sample(g_app_data.hygro.samples_temperature,
		                 g_app_data.hygro.sample_count, &hygro_measurement->temperature);

		aggregate_sample(g_app_data.hygro.samples_humidity, g_app_data.hygro.sample_count,
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
	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

int app_sensor_w1_therm_sample(void)
{
	int ret;

	app_data_lock();

	for (int j = 0; j < MIN(APP_DATA_W1_THERM_COUNT, ctr_ds18b20_get_count()); j++) {
		if (g_app_data.w1_therm.sensor[j].sample_count < APP_DATA_MAX_SAMPLES) {
			int i = g_app_data.w1_therm.sensor[j].sample_count;

			ret = ctr_ds18b20_read(
			        j, &g_app_data.w1_therm.sensor[j].serial_number,
			        &g_app_data.w1_therm.sensor[j].samples_temperature[i]);

			if (ret) {
				g_app_data.w1_therm.sensor[j].samples_temperature[i] = NAN;
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				continue;
			} else {
				LOG_INF("Temperature: %.1f C",
				        g_app_data.w1_therm.sensor[j].samples_temperature[i]);
			}

			g_app_data.w1_therm.sensor[j].sample_count++;

			LOG_INF("Sample count: %d", g_app_data.w1_therm.sensor[j].sample_count);
		} else {
			LOG_WRN("Sample buffer full");
			app_data_unlock();
			return -ENOSPC;
		}
	}

	app_data_unlock();

	return 0;
}

int app_sensor_w1_therm_aggregate(void)
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
		if (w1_therm->sensor[j].measurement_count < APP_DATA_MAX_MEASUREMENTS) {
			struct app_data_w1_therm_measurement *measurement =
			        &w1_therm->sensor[j]
			                 .measurements[w1_therm->sensor[j].measurement_count];

			aggregate_sample(w1_therm->sensor[j].samples_temperature,
			                 w1_therm->sensor[j].sample_count,
			                 &measurement->temperature);

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
