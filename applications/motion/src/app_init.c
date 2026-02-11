/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */
#include "app_config.h"
#include "app_handler.h"
#include "app_work.h"
#include "app_config.h"
#include "app_codec.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_s3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

struct ctr_wdog_channel g_app_wdog_channel;
#if defined(FEATURE_SUBSYSTEM_LTE)
K_SEM_DEFINE(g_app_init_sem, 0, 1);
#endif /* defined(FEATURE_SUBSYSTEM_LTE) */

int app_init(void)
{
	int ret;

    if (g_app_config.service_mode_enabled) {
        ctr_led_set(CTR_LED_CHANNEL_Y, true);
        k_sleep(K_MSEC(50));
        ctr_led_set(CTR_LED_CHANNEL_Y, false);
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

#if defined(FEATURE_SUBSYSTEM_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(CONFIG_CTR_S3)
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s3));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_s3_set_handler(dev, app_handler_ctr_s3, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_s3_set_handler` failed: %d", ret);
		return ret;
	}

	struct ctr_s3_param param;

	switch (g_app_config.sensitivity) {
	case APP_CONFIG_SENSITIVITY_LOW:
		param = (struct ctr_s3_param){ .sensitivity = 32,
					    .blind_time = 3,
					    .pulse_counter = 3,
					    .window_time = 4 };
		LOG_INF("Sensitivity set to low");
		break;
	case APP_CONFIG_SENSITIVITY_HIGH:
		param = (struct ctr_s3_param){ .sensitivity = 32,
					    .blind_time = 1,
					    .pulse_counter = 1,
					    .window_time = 0 };
		LOG_INF("Sensitivity set to high");
		break;
	case APP_CONFIG_SENSITIVITY_INDIVIDUAL:
		param = (struct ctr_s3_param){ .sensitivity = g_app_config.motion_sens,
					    .blind_time = g_app_config.motion_blind,
					    .pulse_counter = g_app_config.motion_pulse,
					    .window_time = g_app_config.motion_window };
		LOG_INF("Sensitivity set to individual (sens=%d, blind=%d, pulse=%d, window=%d)",
			g_app_config.motion_sens, g_app_config.motion_blind,
			g_app_config.motion_pulse, g_app_config.motion_window);
		break;
	case APP_CONFIG_SENSITIVITY_MEDIUM:
	default:
		param = (struct ctr_s3_param){ .sensitivity = 64,
					    .blind_time = 2,
					    .pulse_counter = 2,
					    .window_time = 2 };
		LOG_INF("Sensitivity set to medium");
		break;
	}

	ret = ctr_s3_configure(dev, CTR_S3_CHANNEL_L, &param);
	if (ret) {
		LOG_ERR("Call `ctr_s3_configure` failed: %d", ret);
		return ret;
	}

	ret = ctr_s3_configure(dev, CTR_S3_CHANNEL_R, &param);
	if (ret) {
		LOG_ERR("Call `ctr_s3_configure` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_CTR_S3) */

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

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

	return 0;
}