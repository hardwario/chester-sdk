/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_init.h"
#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

extern struct ctr_wdog_channel g_app_wdog_channel;

static bool should_feed_watchdog(void);

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
		k_sleep(K_SECONDS(5));
		LOG_INF("Alive");

#if defined(FEATURE_HARDWARE_CHESTER_S1)
		app_iaq_led_task();
#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

		switch (g_app_config.mode) {
		case APP_CONFIG_MODE_NONE:
			/* Blink yellow when LTE/LRW mode not configured*/
			ctr_led_set(CTR_LED_CHANNEL_Y, true);
			k_sleep(K_MSEC(30));
			ctr_led_set(CTR_LED_CHANNEL_Y, false);
			break;

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
		case APP_CONFIG_MODE_LTE:
			struct ctr_lte_v2_cereg_param cereg_param;
			ctr_lte_v2_get_cereg_param(&cereg_param);

			enum ctr_led_channel led_channel =
				(cereg_param.stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME ||
				 cereg_param.stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING)
					? CTR_LED_CHANNEL_G
					: CTR_LED_CHANNEL_R;

			ctr_led_set(led_channel, true);
			k_sleep(K_MSEC(30));
			ctr_led_set(led_channel, false);
			break;
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

		default:
			ctr_led_set(CTR_LED_CHANNEL_G, true);
			k_sleep(K_MSEC(30));
			ctr_led_set(CTR_LED_CHANNEL_G, false);
			break;
		}

		if (should_feed_watchdog()) {
			LOG_DBG("WDG feed");

			ret = ctr_wdog_feed(&g_app_wdog_channel);
			if (ret) {
				LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
				k_oops();
			}
		}
	}

	return 0;
}

static bool should_feed_watchdog(void)
{

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
	int ret;

	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {

		if (g_app_config.downlink_wdg_interval) {
			int64_t downlink_ts;
			ret = ctr_cloud_get_last_seen_ts(&downlink_ts);

			if (ret) {
				LOG_WRN("No downlink timestamp");
				return false;
			}

			int64_t current_ts;
			ret = ctr_rtc_get_ts(&current_ts);
			if (ret) {
				LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
				return false;
			}

			int64_t diff_ts = current_ts - downlink_ts;
			if (diff_ts > g_app_config.downlink_wdg_interval) {
				LOG_WRN("Downlink ts bigger than interval! %lld", diff_ts);
				return false;
			}
		}
	}
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

	return true;
}
