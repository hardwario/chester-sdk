/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_modbus.h"
#include "app_serial.h"
#include "app_work.h"
#include "feature.h"

/* Device layer */
#include "app_device.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_wdog.h>

#if defined(FEATURE_SUBSYSTEM_CLOUD)
#include <chester/ctr_cloud.h>
#include "app_codec.h"
#endif

#if defined(FEATURE_SUBSYSTEM_LRW)
#include <chester/ctr_lrw.h>
#endif

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

extern struct ctr_wdog_channel g_app_wdog_channel;

int app_init(void)
{
	int ret;

	/* Turn on red LED to indicate initialization */
	ctr_led_set(CTR_LED_CHANNEL_R, true);

	LOG_INF("Initializing application");

#if FEATURE_SUBSYSTEM_WDOG
	/* Initialize watchdog (120 second timeout) */
	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("ctr_wdog_set_timeout failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_install(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("ctr_wdog_install failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("ctr_wdog_start failed: %d", ret);
		return ret;
	}

	LOG_INF("Watchdog initialized");
#endif

#if FEATURE_SUBSYSTEM_BUTTON
	/* Set up button handler */
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("ctr_button_set_event_cb failed: %d", ret);
		return ret;
	}

	LOG_INF("Button handler initialized");
#endif

#if FEATURE_SUBSYSTEM_CLOUD
	/* Initialize cloud subsystem only if LTE mode */
	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		/* Use generated codec options from app_codec.h */
		CODEC_CLOUD_OPTIONS_STATIC(cloud_options);

		ret = ctr_cloud_init(&cloud_options);
		if (ret) {
			LOG_ERR("ctr_cloud_init failed: %d", ret);
			return ret;
		}

		/* Set cloud callback */
		ret = ctr_cloud_set_callback(app_handler_cloud, NULL);
		if (ret) {
			LOG_ERR("ctr_cloud_set_callback failed: %d", ret);
			return ret;
		}

		/* Wait for cloud to initialize */
		while (true) {
			ret = ctr_cloud_wait_initialized(K_SECONDS(60));
			if (!ret) {
				LOG_INF("Cloud initialized");
				break;
			} else if (ret == -ETIMEDOUT) {
				LOG_INF("Waiting for cloud initialization...");

				/* Feed watchdog while waiting */
				ret = ctr_wdog_feed(&g_app_wdog_channel);
				if (ret) {
					LOG_ERR("ctr_wdog_feed failed: %d", ret);
					return ret;
				}
			} else {
				LOG_ERR("ctr_cloud_wait_initialized failed: %d", ret);
				LOG_WRN("Continuing without cloud connection");
				break;
			}
		}
	}
#endif

#if FEATURE_SUBSYSTEM_LRW
	/* Initialize LoRaWAN if LRW mode */
	if (g_app_config.mode == APP_CONFIG_MODE_LRW) {
		ret = ctr_lrw_init(app_handler_lrw, NULL);
		if (ret) {
			LOG_ERR("ctr_lrw_init failed: %d", ret);
			LOG_WRN("Continuing without LoRaWAN");
		} else {
			ret = ctr_lrw_start(NULL);
			if (ret) {
				LOG_ERR("ctr_lrw_start failed: %d", ret);
				LOG_WRN("Continuing without LoRaWAN");
			} else {
				LOG_INF("LoRaWAN initialized");
			}
		}
	}
#endif

	/* Check if any device is configured */
	bool has_device = false;
	bool has_modbus_device = false;
	bool has_serial_device = false;
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_CONFIG_DEVICE_TYPE_NONE) {
			has_device = true;
			/* MicroSENS uses RS-232 via app_serial */
			if (g_app_config.devices[i].type == APP_CONFIG_DEVICE_TYPE_MICROSENS_180HS) {
				has_serial_device = true;
			} else if (g_app_config.serial_mode == APP_CONFIG_SERIAL_MODE_MODBUS) {
				has_modbus_device = true;
			} else {
				has_serial_device = true;
			}
		}
	}

	/* Initialize serial interface first (needed by device drivers) */
	if (has_serial_device) {
		ret = app_serial_init();
		if (ret) {
			LOG_ERR("app_serial_init failed: %d", ret);
			return ret;
		}
		LOG_INF("Serial interface initialized");
	}

	/* Initialize device drivers (MicroSENS, etc.) */
	ret = app_device_init();
	if (ret && ret != -EIO) {
		LOG_ERR("app_device_init failed: %d", ret);
		return ret;
	}

	/* Initialize Modbus for Modbus device types */
	if (has_modbus_device) {
		ret = app_modbus_init();
		if (ret) {
			LOG_ERR("app_modbus_init failed: %d", ret);
			return ret;
		}
		LOG_INF("Modbus RTU client initialized");
	}

	if (!has_device) {
		LOG_INF("No device configured");
	}

	/* Note: Transmission mode (LTE/LRW/NONE) was already initialized above */
	/* Data collection works independently, transmission depends on mode */

	/* Initialize work queue */
	ret = app_work_init();
	if (ret) {
		LOG_ERR("app_work_init failed: %d", ret);
		return ret;
	}

	/* Turn off red LED - initialization complete */
	ctr_led_set(CTR_LED_CHANNEL_R, false);

	LOG_INF("Application initialized successfully");

	return 0;
}
