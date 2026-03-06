/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_handler.h"
#include "app_work.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>

#if FEATURE_SUBSYSTEM_CLOUD
#include <chester/ctr_cloud.h>
#endif

#if FEATURE_SUBSYSTEM_LRW
#include <chester/ctr_lrw.h>
#endif

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

#if FEATURE_SUBSYSTEM_LRW
/* Delayed work for LRW retries (non-blocking alternative to k_sleep) */
static void lrw_start_retry_handler(struct k_work *work);
static void lrw_join_retry_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(m_lrw_start_retry_work, lrw_start_retry_handler);
static K_WORK_DELAYABLE_DEFINE(m_lrw_join_retry_work, lrw_join_retry_handler);

static void lrw_start_retry_handler(struct k_work *work)
{
	LOG_INF("Retrying LoRaWAN start...");
	ctr_lrw_start(NULL);
}

static void lrw_join_retry_handler(struct k_work *work)
{
	LOG_INF("Retrying LoRaWAN join...");
	ctr_lrw_join(NULL);
}
#endif /* FEATURE_SUBSYSTEM_LRW */

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
		/* Visual feedback - blink yellow LED for each click */
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
			LOG_INF("Button: 1 click - send data");
			app_work_send();
			break;
		case 2:
			LOG_INF("Button: 2 clicks - sample");
			app_work_sample();
			break;
		case 3:
			LOG_INF("Button: 3 clicks - sample + send");
			app_work_sample();
			app_work_send();
			break;
		case 4:
			LOG_INF("Button: 4 clicks - reboot");
			sys_reboot(SYS_REBOOT_COLD);
			break;
		case 5:
			LOG_INF("Button: 5 clicks - LED load 2min");
			ret = ctr_led_set(CTR_LED_CHANNEL_LOAD, 1);
			if (ret) {
				LOG_ERR("Call `ctr_led_set` failed: %d", ret);
				return;
			}
			k_timer_start(&app_load_timer, K_MINUTES(2), K_FOREVER);
			break;
		default:
			LOG_WRN("Button: %d clicks - ignored", val);
			break;
		}
	}
}

#if FEATURE_SUBSYSTEM_CLOUD
void app_handler_cloud(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
		       void *user_data)
{
	switch (event) {
	case CTR_CLOUD_EVENT_RECV:
		LOG_INF("Cloud event: Downlink received (length: %zu)", data->recv.len);
		/* Handle downlink messages here */
		break;
	default:
		LOG_WRN("Unknown cloud event: %d", event);
		break;
	}
}
#endif

#if FEATURE_SUBSYSTEM_LRW
void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param)
{
	switch (event) {
	case CTR_LRW_EVENT_START_OK:
		LOG_INF("LoRaWAN event: Start OK - joining network");
		ctr_lrw_join(NULL);
		break;

	case CTR_LRW_EVENT_START_ERR:
		LOG_ERR("LoRaWAN event: Start error");
		/* Retry start after 60 seconds (non-blocking) */
		k_work_schedule(&m_lrw_start_retry_work, K_SECONDS(60));
		break;

	case CTR_LRW_EVENT_JOIN_OK:
		LOG_INF("LoRaWAN event: Join OK");
		break;

	case CTR_LRW_EVENT_JOIN_ERR:
		LOG_ERR("LoRaWAN event: Join error");
		/* Retry join after 60 seconds (non-blocking) */
		k_work_schedule(&m_lrw_join_retry_work, K_SECONDS(60));
		break;

	case CTR_LRW_EVENT_SEND_OK:
		LOG_INF("LoRaWAN event: Send OK");
		break;

	case CTR_LRW_EVENT_SEND_ERR:
		LOG_ERR("LoRaWAN event: Send error");
		break;

	case CTR_LRW_EVENT_FAILURE:
		LOG_ERR("LoRaWAN event: Failure - restarting");
		/* Restart LoRaWAN after 60 seconds (non-blocking) */
		k_work_schedule(&m_lrw_start_retry_work, K_SECONDS(60));
		break;

	default:
		LOG_WRN("Unknown LoRaWAN event: %d", event);
		break;
	}
}
#endif
