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
#include <chester/ctr_cloud.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/hwinfo.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

#if defined(FEATURE_SUBSYSTEM_WDOG)
struct ctr_wdog_channel g_app_wdog_channel;
#endif /* defined(FEATURE_SUBSYSTEM_WDOG) */

K_THREAD_STACK_DEFINE(led_thread_stack, 512);
struct k_thread led_thread_data;

void led_thread(void *param1, void *param2, void *param3)
{
	k_thread_name_set(NULL, "led_thread");

	for (;;) {

		if (atomic_set(&g_app_data_send_flag, false)) {
			ctr_led_set(CTR_LED_CHANNEL_R, true);
			k_sleep(K_MSEC(500));
			ctr_led_set(CTR_LED_CHANNEL_R, false);
			k_sleep(K_MSEC(500));
		}

		if (atomic_get(&g_app_data_working_flag)) {
			/* Faster yellow blink when scanning */
			ctr_led_set(CTR_LED_CHANNEL_Y, true);
			k_sleep(K_MSEC(50));
			ctr_led_set(CTR_LED_CHANNEL_Y, false);

			/* When dual antenna scan is used and second antenna is scanning, blink
			 * twice*/
			if (g_app_config.scan_ant == 1 && !g_app_data_antenna_dual) {
				k_sleep(K_MSEC(200));

				ctr_led_set(CTR_LED_CHANNEL_Y, true);
				k_sleep(K_MSEC(50));
				ctr_led_set(CTR_LED_CHANNEL_Y, false);
				k_sleep(K_MSEC(50));
			}

			k_sleep(K_MSEC(2000));
		} else {
			/* Slow green blink when attached */
			ctr_led_set(CTR_LED_CHANNEL_G, true);
			k_sleep(K_MSEC(50));
			ctr_led_set(CTR_LED_CHANNEL_G, false);
			k_sleep(K_MSEC(5000));
		}
	}
}

int app_init(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	uint32_t cause;

	ret = hwinfo_get_reset_cause(&cause);
	if (ret) {
		LOG_ERR("Call `hwinfo_get_reset_cause` failed: %d", ret);
		return ret;
	}

	LOG_INF("Reset reason 0x%02X", cause);

	ret = hwinfo_clear_reset_cause();
	if (ret) {
		LOG_ERR("Call `hwinfo_clear_reset_cause` failed: %d", ret);
		return ret;
	}

	ret = wmbus_init();
	if (ret) {
		LOG_ERR("Call `wmbus_init` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_SUBSYSTEM_WDOG)
	ret = ctr_wdog_set_timeout(20 * 60 * 1000);
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
#endif /* defined(FEATURE_SUBSYSTEM_WDOG) */

#if defined(FEATURE_SUBSYSTEM_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

	switch (g_app_config.mode) {
#if defined(FEATURE_SUBSYSTEM_CLOUD)
	case APP_CONFIG_MODE_LTE:
		CODEC_CLOUD_OPTIONS_STATIC(copt);

		ret = ctr_cloud_init(&copt);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
			return ret;
		}

		if (g_app_config.poll_interval) {
			ret = ctr_cloud_set_poll_interval(K_SECONDS(g_app_config.poll_interval));
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
		break;
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */
	default:
		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

	k_thread_create(&led_thread_data, led_thread_stack, K_THREAD_STACK_SIZEOF(led_thread_stack),
			led_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	return 0;
}