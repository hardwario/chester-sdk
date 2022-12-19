#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"
#include "app_measure.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

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

	bool error = false;

	ret = ctr_accel_read(&g_app_data.states.acceleration_x, &g_app_data.states.acceleration_y,
			     &g_app_data.states.acceleration_z, &g_app_data.states.orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		g_app_data.errors.orientation = true;
		g_app_data.errors.acceleration_x = true;
		g_app_data.errors.acceleration_y = true;
		g_app_data.errors.acceleration_z = true;
		error = true;
	} else {
		LOG_INF("Orientation: %d", g_app_data.states.orientation);
		g_app_data.errors.orientation = false;
		g_app_data.errors.acceleration_x = false;
		g_app_data.errors.acceleration_y = false;
		g_app_data.errors.acceleration_z = false;
	}

	ret = ctr_therm_read(&g_app_data.states.int_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		g_app_data.errors.int_temperature = true;
		error = true;
	} else {
		LOG_INF("Int. temperature: %.1f C", g_app_data.states.int_temperature);
		g_app_data.errors.int_temperature = false;
	}

#if defined(CONFIG_CTR_HYGRO)
	ret = ctr_hygro_read(&g_app_data.states.ext_temperature, &g_app_data.states.ext_humidity);
	if (ret) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		g_app_data.errors.ext_temperature = true;
		g_app_data.errors.ext_humidity = true;
		error = true;
	} else {
		LOG_INF("Ext. temperature: %.1f C", g_app_data.states.ext_temperature);
		LOG_INF("Ext. humidity: %.1f %%", g_app_data.states.ext_humidity);
		g_app_data.errors.ext_temperature = false;
		g_app_data.errors.ext_humidity = false;
	}
#else
	g_app_data.errors.ext_temperature = true;
	g_app_data.errors.ext_humidity = true;
#endif

	return error ? -EIO : 0;
}
