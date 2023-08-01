/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lte.h>
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

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(CONFIG_SHIELD_CTR_Z)
static int init_ctr_z(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_z_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
		return ret;
	}

	return 0;
}
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
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

	return 0;
}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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

#if defined(CONFIG_CTR_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_CTR_BUTTON) */

#if defined(CONFIG_SHIELD_CTR_Z)
	ret = init_ctr_z();
	if (ret) {
		LOG_ERR("Call `init_ctr_z` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	ret = init_people_counter();
	if (ret) {
		LOG_ERR("Call `init_people_counter` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	ret = ctr_lte_set_event_cb(app_handler_lte, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		return ret;
	}

	for (;;) {
		ret = ctr_wdog_feed(&g_app_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			return ret;
		}

		ret = k_sem_take(&g_app_init_sem, K_SECONDS(1));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			return ret;
		}

		break;
	}

	ret = ctr_rtc_get_ts(&g_app_data.weight_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	ret = ctr_rtc_get_ts(&g_app_data.people_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	return 0;
}
