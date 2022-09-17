#include "app_measure.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_k.h>

/* Zephyr includes */
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

static int measure_channels(void)
{
	int ret;
	int err = 0;

	if (g_app_data.channel_measurement_count >= ARRAY_SIZE(g_app_data.channel_measurements)) {
		LOG_WRN("Channel measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	enum ctr_k_channel channels[APP_CONFIG_CHANNEL_COUNT] = { 0 };
	struct ctr_k_calibration calibrations[APP_CONFIG_CHANNEL_COUNT] = { 0 };

	size_t channels_count = 0;

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		calibrations[i].x0 = NAN;
		calibrations[i].y0 = NAN;
		calibrations[i].x1 = NAN;
		calibrations[i].y1 = NAN;

		if (!g_app_config.channel_active[i]) {
			continue;
		}

		if (g_app_config.channel_differential[i]) {
			channels[channels_count] = CTR_K_CHANNEL_DIFFERENTIAL(i + 1);
		} else {
			channels[channels_count] = CTR_K_CHANNEL_SINGLE_ENDED(i + 1);
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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_k));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	for (size_t i = 0; i < channels_count; i++) {
		ret = ctr_k_set_power(dev, channels[i], true);
		if (ret) {
			LOG_ERR("Call `ctr_k_set_power` (%u) failed: %d", i, ret);
			err = ret;
			goto error;
		}
	}

	k_sleep(K_MSEC(100));

	struct ctr_k_result results[ARRAY_SIZE(channels)] = { 0 };
	ret = ctr_k_measure(dev, channels, channels_count, calibrations, results);
	if (ret) {
		LOG_ERR("Call `ctr_k_measure` failed: %d", ret);
		err = ret;
		goto error;
	}

	struct app_data_channel_measurement *m =
	        &g_app_data.channel_measurements[g_app_data.channel_measurement_count++];

	m->timestamp_offset = timestamp - g_app_data.channel_measurement_timestamp;

	for (size_t i = 0; i < APP_CONFIG_CHANNEL_COUNT; i++) {
		m->avg[i] = INT_MAX;
		m->rms[i] = INT_MAX;
	}

	for (size_t i = 0; i < channels_count; i++) {
		m->avg[CTR_K_CHANNEL_IDX(channels[i])] = results[i].avg;
		m->rms[CTR_K_CHANNEL_IDX(channels[i])] = results[i].rms;
	}

error:

	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		ret = ctr_k_set_power(dev, channels[i], false);
		if (ret) {
			LOG_ERR("Call `ctr_k_set_power` (%u) failed: %d", i, ret);
			err = err ? err : ret;
		}
	}

	return err;
}

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

	ret = ctr_accel_read(&g_app_data.acceleration_x, &g_app_data.acceleration_y,
	                     &g_app_data.acceleration_z, &g_app_data.orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		g_app_data.acceleration_x = NAN;
		g_app_data.acceleration_y = NAN;
		g_app_data.acceleration_z = NAN;
		g_app_data.orientation = INT_MAX;
	}

	ret = measure_channels();
	if (ret) {
		LOG_ERR("Call `measure_channels` failed: %d", ret);
	}

	return 0;
}
