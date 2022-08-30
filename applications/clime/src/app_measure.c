#include "app_measure.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"
#include "app_send.h"

/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_ds18b20.h>
#include <ctr_hygro.h>
#include <ctr_rtc.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_measure, LOG_LEVEL_DBG);

static void measure_timer(struct k_timer *timer_id)
{
	LOG_INF("Measure timer expired");

	atomic_set(&g_app_loop_measure, true);
	k_sem_give(&g_app_loop_sem);
}

K_TIMER_DEFINE(g_app_measure_timer, measure_timer, NULL);

int app_measure(void)
{
	int ret;

	k_timer_start(&g_app_measure_timer, K_MSEC(g_app_config.measurement_interval * 1000),
	              K_FOREVER);

	ret = ctr_therm_read(&g_app_data.therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		g_app_data.therm_temperature = NAN;
	}

#if defined(CONFIG_SHIELD_CTR_S1)
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

	ret = ctr_s1_temperature_read(dev, &g_app_data.s1_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_s1_temperature_read` failed: %d", ret);
	} else {
		LOG_INF("S1 temperature: %.1f C", g_app_data.s1_temperature);
	}

	ret = ctr_s1_humidity_read(dev, &g_app_data.s1_humidity);
	if (ret) {
		LOG_ERR("Call `ctr_s1_humidity_read` failed: %d", ret);
	} else {
		LOG_INF("S1 humidity: %.1f RH", g_app_data.s1_humidity);
	}

	ret = ctr_s1_co2_concentration_read(dev, &g_app_data.s1_co2_concentration);
	if (ret) {
		LOG_ERR("Call `ctr_s1_co2_concentration_read` failed: %d", ret);
	} else {
		LOG_INF("S1 co2 concentration: %.0f ppm", g_app_data.s1_co2_concentration);
	}

	ret = ctr_s1_illuminance_read(dev, &g_app_data.s1_illuminance);
	if (ret) {
		LOG_ERR("Call `ctr_s1_illuminance_read` failed: %d", ret);
	} else {
		LOG_INF("S1 illuminance: %.0f lux", g_app_data.s1_illuminance);
	}

	ret = ctr_s1_pressure_read(dev, &g_app_data.s1_pressure);
	if (ret) {
		LOG_ERR("Call `ctr_s1_pressure_read` failed: %d", ret);
	} else {
		LOG_INF("S1 pressure: %.0f Pa", g_app_data.s1_pressure);
	}

	ret = ctr_s1_altitude_read(dev, &g_app_data.s1_altitude);
	if (ret) {
		LOG_ERR("Call `ctr_s1_altitude_read` failed: %d", ret);
	} else {
		LOG_INF("S1 altitude: %.0f m", g_app_data.s1_altitude);
	}
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	ret = ctr_hygro_read(&g_app_data.hygro_temperature, &g_app_data.hygro_humidity);
	if (ret) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		g_app_data.hygro_temperature = NAN;
		g_app_data.hygro_humidity = NAN;
	}
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

	ret = ctr_accel_read(&g_app_data.accel_x, &g_app_data.accel_y, &g_app_data.accel_z,
	                     &g_app_data.accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		g_app_data.accel_x = NAN;
		g_app_data.accel_y = NAN;
		g_app_data.accel_z = NAN;
		g_app_data.accel_orientation = INT_MAX;
	}

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	for (int i = 0; i < MIN(W1_THERM_COUNT, ctr_ds18b20_get_count()); i++) {
		struct w1_therm *therm = &g_app_data.w1_therm[i];

		if (therm->sample_count >= ARRAY_SIZE(therm->samples)) {
			LOG_WRN("1-Wire thermometer %u: Sample buffer full", i);
			continue;
		}

		if (!therm->sample_count) {
			ret = ctr_rtc_get_ts(&therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				continue;
			}
		}

		uint64_t serial_number;
		float temperature;
		ret = ctr_ds18b20_read(i, &serial_number, &temperature);
		if (ret) {
			LOG_WRN("Call `ctr_ds18b20_read` failed: %d", ret);
			continue;
		}

		therm->serial_number = serial_number;
		therm->samples[therm->sample_count++] = temperature;

		LOG_INF("1-Wire thermometer %u: Temperature: %.2f", i, temperature);
	}
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

	return 0;
}
