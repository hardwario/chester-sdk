/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_z.h>
#include <chester/drivers/people_counter.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER)

static int init_people_counter(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(people_counter));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = people_counter_set_power_off_delay(dev, g_app_config.people_counter_power_off_delay);
	if (ret) {
		LOG_ERR("Call `people_counter_set_power_off_delay` failed: %d", ret);
		return ret;
	}

	ret = people_counter_set_stay_timeout(dev, g_app_config.people_counter_stay_timeout);
	if (ret) {
		LOG_ERR("Call `people_counter_set_stay_timeout` failed: %d", ret);
		return ret;
	}

	ret = people_counter_set_adult_border(dev, g_app_config.people_counter_adult_border);
	if (ret) {
		LOG_ERR("Call `people_counter_set_adult_border` failed: %d", ret);
		return ret;
	}

	int64_t people_measurement_ts;
	ret = ctr_rtc_get_ts(&people_measurement_ts);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}
	app_data_lock();
	g_app_data.people_measurement_timestamp = people_measurement_ts;
	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER) */

int app_init(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);
	k_sleep(K_SECONDS(2));
	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_install(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_SUBSYSTEM_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(FEATURE_SUBSYSTEM_CLOUD)
	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		CODEC_CLOUD_OPTIONS_STATIC(copt);

		ret = ctr_cloud_init(&copt);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
			return ret;
		}

		if (g_app_config.interval_poll > 0) {
			ret = ctr_cloud_set_poll_interval(K_SECONDS(g_app_config.interval_poll));
			if (ret) {
				LOG_ERR("Call `ctr_cloud_set_pull_interval` failed: %d", ret);
				return ret;
			}
		}

		while (true) {
			ret = ctr_cloud_wait_initialized(K_SECONDS(60));
			if (!ret) {
				break;
			} else {
				if (ret == -ETIMEDOUT) {
					LOG_INF("Waiting for cloud initialization");
				} else {
					LOG_ERR("Call `ctr_cloud_wait_initialized` failed: %d",
						ret);
					return ret;
				}
			}

			ret = ctr_wdog_feed(&g_app_wdog_channel);
			if (ret) {
				LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
				return ret;
			}
		}
	}
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */

	ctr_led_set(CTR_LED_CHANNEL_R, false);

#if defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER)
	ret = init_people_counter();
	if (ret) {
		LOG_ERR("Call `init_people_counter` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER) */

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}
	app_data_lock();
	g_app_data.weight_measurement_timestamp = timestamp;
	app_data_unlock();

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	ret = app_backup_init();
	if (ret) {
		LOG_ERR("Call `app_backup_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

	return 0;
}
