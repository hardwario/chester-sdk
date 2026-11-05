/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_power.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

int app_init(void)
{
	int ret;

	ret = app_data_init();
	if (ret) {
		LOG_ERR("Call `app_data_init` failed: %d", ret);
		return ret;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, true);

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

	switch (g_app_config.mode) {
		
#if defined(FEATURE_SUBSYSTEM_LRW)
	case APP_CONFIG_MODE_LRW:
		ret = ctr_lrw_init(app_handler_lrw, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_init` failed: %d", ret);
			return ret;
		}

		ret = ctr_lrw_start(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
			return ret;
		}

		k_sleep(K_SECONDS(2));
		break;
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

#if defined(FEATURE_SUBSYSTEM_CLOUD)
	case APP_CONFIG_MODE_LTE:
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
					LOG_ERR("Call `ctr_cloud_wait_initialized` failed: %d", ret);
					return ret;
				}
			}

			ret = ctr_wdog_feed(&g_app_wdog_channel);
			if (ret) {
				LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
				return ret;
			}
		}
		break;
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */

	default:
		break;
	}

#if defined(FEATURE_SUBSYSTEM_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

	ctr_led_set(CTR_LED_CHANNEL_R, false);

#if defined(FEATURE_SUBSYSTEM_POWER)
	ret = ctr_power_init(app_handler_power_event, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_power_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_POWER) */

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

	return 0;
}
