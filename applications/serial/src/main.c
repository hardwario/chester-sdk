/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include "app_config.h"
#include "app_init.h"

#include <chester/ctr_led.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

/* Watchdog channel */
struct ctr_wdog_channel g_app_wdog_channel;

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	/* Initialize application */
	ret = app_init();
	if (ret) {
		LOG_ERR("Application initialization failed: %d", ret);
		k_oops();
	}

	/* Main loop */
	for (;;) {
		k_sleep(K_SECONDS(5));
		LOG_INF("Alive");

		/* Feed watchdog */
		ctr_wdog_feed(&g_app_wdog_channel);

		/* Blink LED based on configuration status */
		if (g_app_config.mode == APP_CONFIG_MODE_NONE) {
			/* Blink yellow LED when not configured */
			ctr_led_set(CTR_LED_CHANNEL_Y, true);
			k_sleep(K_MSEC(50));
			ctr_led_set(CTR_LED_CHANNEL_Y, false);
		} else {
			/* Blink green LED to indicate normal operation */
			ctr_led_set(CTR_LED_CHANNEL_G, true);
			k_sleep(K_MSEC(50));
			ctr_led_set(CTR_LED_CHANNEL_G, false);
		}
	}

	return 0;
}
