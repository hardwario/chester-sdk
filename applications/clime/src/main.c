/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_iaq.h"
#include "app_init.h"

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

extern struct ctr_wdog_channel g_app_wdog_channel;

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = app_init();
	if (ret) {
		LOG_ERR("Call `app_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_wdog_feed(&g_app_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

#if defined(FEATURE_HARDWARE_CHESTER_S1)
		app_iaq_led_task();
#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

		/* Blink yellow when LTE/LRW mode not configured*/
		if (g_app_config.mode == APP_CONFIG_MODE_NONE) {
			ctr_led_set(CTR_LED_CHANNEL_Y, true);
			k_sleep(K_MSEC(30));
			ctr_led_set(CTR_LED_CHANNEL_Y, false);
		} else {
			ctr_led_set(CTR_LED_CHANNEL_G, true);
			k_sleep(K_MSEC(30));
			ctr_led_set(CTR_LED_CHANNEL_G, false);
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
