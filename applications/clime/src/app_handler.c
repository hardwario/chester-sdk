/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_x4.h>
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

void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param)
{
	int ret;

	switch (event) {
	case CTR_LRW_EVENT_FAILURE:
		LOG_INF("Event `CTR_LRW_EVENT_FAILURE`");
		ret = ctr_lrw_start(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_OK:
		LOG_INF("Event `CTR_LRW_EVENT_START_OK`");
		ret = ctr_lrw_join(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_join` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_START_ERR`");
		break;
	case CTR_LRW_EVENT_JOIN_OK:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_OK`");
		break;
	case CTR_LRW_EVENT_JOIN_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_ERR`");
		break;
	case CTR_LRW_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_OK`");
		break;
	case CTR_LRW_EVENT_SEND_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_ERR`");
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

#if defined(FEATURE_HARDWARE_CHESTER_S1)
void ctr_s1_event_handler(const struct device *dev, enum ctr_s1_event event, void *user_data)
{
	int ret;

	switch (event) {
	case CTR_S1_EVENT_DEVICE_RESET:
		LOG_INF("Event `CTR_S1_EVENT_DEVICE_RESET`");

		ret = ctr_s1_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
			k_oops();
		}

		ret = ctr_s1_set_motion_sensitivity(dev, CTR_S1_PIR_SENSITIVITY_MEDIUM);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_motion_sensitivity` failed: %d", ret);
			k_oops();
		}

		break;

	case CTR_S1_EVENT_BUTTON_PRESSED:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_PRESSED`");

		atomic_inc(&g_app_data.iaq.button.press_count);

		struct ctr_s1_led_param param_led = {
			.brightness = CTR_S1_LED_BRIGHTNESS_HIGH,
			.command = CTR_S1_LED_COMMAND_1X_1_1,
			.pattern = CTR_S1_LED_PATTERN_OFF,
		};

		ret = ctr_s1_set_led(dev, CTR_S1_LED_CHANNEL_B, &param_led);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
			k_oops();
		}

		struct ctr_s1_buzzer_param param_buzzer = {
			.command = CTR_S1_BUZZER_COMMAND_1X_1_1,
			.pattern = CTR_S1_BUZZER_PATTERN_OFF,
		};

		ret = ctr_s1_set_buzzer(dev, &param_buzzer);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_buzzer` failed: %d", ret);
			k_oops();
		}

		ret = ctr_s1_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
			k_oops();
		}

		app_sensor_iaq_button_aggreg();

		app_work_send();

		break;

	case CTR_S1_EVENT_BUTTON_CLICKED:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_CLICKED`");
		break;

	case CTR_S1_EVENT_BUTTON_HOLD:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_HOLD`");
		break;

	case CTR_S1_EVENT_BUTTON_RELEASED:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_RELEASED`");
		break;

	case CTR_S1_EVENT_MOTION_DETECTED:
		LOG_INF("Event `CTR_S1_EVENT_MOTION_DETECTED`");

		atomic_inc(&g_app_data.iaq.sensors.motion_count);

		int motion_count;
		ret = ctr_s1_read_motion_count(dev, &motion_count);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_motion_count` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Motion count: %d", motion_count);

		break;

	default:
		break;
	}
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

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

#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)

void app_handler_ctr_x4(const struct device *dev, enum ctr_x4_event event, void *user_data)
{
	switch (event) {
	case CTR_X4_EVENT_LINE_CONNECTED:
		LOG_INF("Line connected");
		break;
	case CTR_X4_EVENT_LINE_DISCONNECTED:
		LOG_INF("Line disconnected");
		break;
	default:
		LOG_ERR("Unknown event: %d", event);
		k_oops();
	}

	int ret = app_send_lrw_x4_line_alert(event == CTR_X4_EVENT_LINE_CONNECTED ? BIT(0) : 0);
	if (ret) {
		LOG_ERR("Call `app_send_lrw_x4_line_alert` failed: %d", ret);
		return;
	}
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)*/
