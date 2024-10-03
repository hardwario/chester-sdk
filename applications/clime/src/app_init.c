/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_tamper.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_s1.h>
#include <chester/drivers/ctr_x4.h>
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

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)

int init_chester_x4(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (g_app_config.mode == APP_CONFIG_MODE_LRW) {
		ret = ctr_x4_set_handler(dev, app_handler_ctr_x4, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_handler` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B) */

int app_init(void)
{
	int ret;

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
	CODEC_CLOUD_OPTIONS_STATIC(copt);
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

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

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	ret = ctr_soil_sensor_scan();
	if (ret) {
		LOG_ERR("Call `ctr_soil_sensor_scan` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_HARDWARE_CHESTER_S1)
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_s1_set_handler(dev, ctr_s1_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_s1_set_handler` failed: %d", ret);
		return ret;
	}

	ret = ctr_s1_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_s1_enable_interrupts` failed: %d", ret);
		return ret;
	}

#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

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

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
	case APP_CONFIG_MODE_LTE:

		ret = ctr_cloud_init(&copt);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
			return ret;
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
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

	default:
		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_CHESTER_APP_TAMPER)
	ret = app_tamper_init();
	if (ret) {
		LOG_ERR("Call `app_tamper_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_CHESTER_APP_TAMPER) */

#if defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X10)
	ret = app_backup_init();
	if (ret) {
		LOG_ERR("Call `app_backup_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_HARDWARE_CHESTER_X10) */

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)
	ret = init_chester_x4();
	if (ret) {
		LOG_ERR("Call `init_chester_x4` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B) */

	return 0;
}
