/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_BUTTON)

static void app_load_timer_handler(struct k_timer *timer)
{
	ctr_led_set(CTR_LED_CHANNEL_LOAD, 0);
}

K_TIMER_DEFINE(app_load_timer, app_load_timer_handler, NULL);

void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data)
{
	int ret;

	if (chan != CTR_BUTTON_CHANNEL_INT) {
		return;
	}

	if (ev == CTR_BUTTON_EVENT_CLICK) {
		for (int i = 0; i < val; i++) {
			ret = ctr_led_set(CTR_LED_CHANNEL_Y, true);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}

			k_sleep(K_MSEC(50));
			ret = ctr_led_set(CTR_LED_CHANNEL_Y, false);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}

			k_sleep(K_MSEC(200));
		}

		switch (val) {
		case 1:
			app_work_send();
			break;
		case 2:
			app_work_sample();
			break;
		case 3:
			app_work_sample();
			app_work_send();
			break;
		case 4:
			sys_reboot(SYS_REBOOT_COLD);
			break;
		case 5:
			ret = ctr_led_set(CTR_LED_CHANNEL_LOAD, 1);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
			k_timer_start(&app_load_timer, K_MINUTES(2), K_FOREVER);
			break;
		}
	}
}

#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */
