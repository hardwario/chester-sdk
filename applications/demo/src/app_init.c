/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"
#include "app_codec.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_s1.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

struct ctr_wdog_channel g_app_wdog_channel;

int app_init(void)
{
	int ret;

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

	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_SUBSYSTEM_LTE_V2)

	CODEC_CLOUD_OPTIONS_STATIC(copt);

	ret = ctr_cloud_init(&copt);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_cloud_set_callback(app_handler_cloud_event, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_set_callback` failed: %d", ret);
	}

	if (g_app_config.interval_poll) {
		ret = ctr_cloud_set_poll_interval(K_SECONDS(g_app_config.interval_poll));
		if (ret) {
			LOG_ERR("Call `ctr_cloud_set_poll_interval` failed: %d", ret);
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

#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

	return 0;
}
