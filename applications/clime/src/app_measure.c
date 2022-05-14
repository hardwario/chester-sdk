#include "app_measure.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"
#include "app_send.h"

/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_rtc.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <drivers/sensor.h>
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_measure, LOG_LEVEL_DBG);

extern const struct device *g_app_ds18b20_dev[W1_THERM_COUNT];

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

	ret = ctr_accel_read(&g_app_data.accel_x, &g_app_data.accel_y, &g_app_data.accel_z,
	                     &g_app_data.accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		g_app_data.accel_x = NAN;
		g_app_data.accel_y = NAN;
		g_app_data.accel_z = NAN;
		g_app_data.accel_orientation = INT_MAX;
	}

	for (size_t i = 0; i < W1_THERM_COUNT; i++) {
		struct w1_therm *therm = &g_app_data.w1_therm[i];

		if (therm->sample_count >= ARRAY_SIZE(therm->samples)) {
			LOG_WRN("1-Wire thermometer %u: Sample buffer full", i);
			continue;
		}

		if (therm->sample_count == 0) {
			ret = ctr_rtc_get_ts(&therm->timestamp);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				continue;
			}
		}

		if (!therm->serial_number) {
			continue;
		}

		ret = sensor_sample_fetch(g_app_ds18b20_dev[i]);
		if (ret) {
			LOG_WRN("Call `sensor_sample_fetch` failed: %d", ret);
			continue;
		}

		struct sensor_value val;
		ret = sensor_channel_get(g_app_ds18b20_dev[i], SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret) {
			LOG_WRN("Call `sensor_channel_get` failed: %d", ret);
			continue;
		}

		float temperature = (float)val.val1 + (float)val.val2 / 1000000.f;
		therm->samples[therm->sample_count++] = temperature;

		LOG_INF("1-Wire thermometer %u: Temperature: %.2f", i, temperature);
	}

	return 0;
}
