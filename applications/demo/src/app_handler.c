/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_handler.h"
#include "app_config.h"
#include "app_data.h"
#include "app_init.h"
#include "app_work.h"
#include "app_cbor.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

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
			app_work_aggreg();
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

void app_handler_cloud_event(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param)
{
	int ret;

	if (event == CTR_CLOUD_EVENT_RECV) {
		LOG_HEXDUMP_INF(data->recv.buf, data->recv.len, "Received:");

		if (data->recv.len < 8) {
			LOG_ERR("Missing encoder hash");
			return;
		}

		uint8_t *buf = (uint8_t *)data->recv.buf + 8;
		size_t len = data->recv.len - 8;

		ZCBOR_STATE_D(zs, 1, buf, len, 1);

		struct app_cbor_received received;

		ret = app_cbor_decode(zs, &received);
		if (ret) {
			LOG_ERR("Call `app_cbor_decode` failed: %d", ret);
			return;
		}

		if (received.has_led_r) {
			LOG_INF("Set LED R to %d", received.led_r);
			ret = ctr_led_set(CTR_LED_CHANNEL_R, received.led_r);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
		}

		if (received.has_led_g) {
			LOG_INF("Set LED G to %d", received.led_g);
			ret = ctr_led_set(CTR_LED_CHANNEL_G, received.led_g);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
		}

		if (received.has_led_y) {
			LOG_INF("Set LED Y to %d", received.led_y);
			ret = ctr_led_set(CTR_LED_CHANNEL_Y, received.led_y);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
		}

		if (received.has_led_load) {
			LOG_INF("Set LED LOAD to %d", received.led_load);
			ret = ctr_led_set(CTR_LED_CHANNEL_LOAD, received.led_load);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
		}
	}
}
