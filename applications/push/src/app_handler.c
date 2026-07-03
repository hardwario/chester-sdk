/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

#if defined(FEATURE_HARDWARE_CHESTER_Z)
static atomic_t m_report_rate_hourly_counter = 0;
static atomic_t m_report_rate_timer_is_active = false;
static atomic_t m_report_delay_timer_is_active = false;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_LRW)

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

#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)

static void report_delay_timer_handler(struct k_timer *timer)
{
	app_work_send();
	atomic_inc(&m_report_rate_hourly_counter);
	atomic_set(&m_report_delay_timer_is_active, false);
}

static K_TIMER_DEFINE(m_report_delay_timer, report_delay_timer_handler, NULL);

static void report_rate_timer_handler(struct k_timer *timer)
{
	atomic_set(&m_report_rate_hourly_counter, 0);
	atomic_set(&m_report_rate_timer_is_active, false);
}

static K_TIMER_DEFINE(m_report_rate_timer, report_rate_timer_handler, NULL);

static void send_with_rate_limit(void)
{
	if (!atomic_get(&m_report_rate_timer_is_active)) {
		k_timer_start(&m_report_rate_timer, K_HOURS(1), K_NO_WAIT);
		atomic_set(&m_report_rate_timer_is_active, true);
	}

	LOG_INF("Hourly counter state: %d/%d", (int)atomic_get(&m_report_rate_hourly_counter),
		g_app_config.event_report_rate);

	if (atomic_get(&m_report_rate_hourly_counter) <= g_app_config.event_report_rate) {
		if (!atomic_get(&m_report_delay_timer_is_active)) {
			LOG_INF("Starting delay timer");
			k_timer_start(&m_report_delay_timer,
				      K_SECONDS(g_app_config.event_report_delay), K_NO_WAIT);
			atomic_set(&m_report_delay_timer_is_active, true);
		} else {
			LOG_INF("Delay timer already running");
		}
	} else {
		LOG_WRN("Hourly counter exceeded");
	}
}

void handle_dc_event(enum ctr_z_event backup_event)
{
	int ret;

	struct app_data_backup *backup = &g_app_data.backup;

	app_data_lock();

	backup->line_present = backup_event == CTR_Z_EVENT_DC_CONNECTED;

	if (backup->event_count < APP_DATA_MAX_BACKUP_EVENTS) {
		struct app_data_backup_event *event = &backup->events[backup->event_count];

		ret = ctr_rtc_get_ts(&event->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return;
		}

		event->connected = backup->line_present;
		backup->event_count++;

		LOG_INF("Event count: %d", backup->event_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return;
	}

	LOG_INF("Backup: %d", (int)backup->line_present);

	if (g_app_config.backup_report_connected && backup_event == CTR_Z_EVENT_DC_CONNECTED) {
		send_with_rate_limit();
	}

	if (g_app_config.backup_report_disconnected &&
	    backup_event == CTR_Z_EVENT_DC_DISCONNECTED) {
		send_with_rate_limit();
	}

	app_data_unlock();
}

struct app_handler_button_action {
	enum ctr_z_event event;
	enum ctr_z_led_channel led_channel;
	struct ctr_z_led_param led_param;
	enum ctr_z_buzzer_command buzzer_command;
	bool is_hold;
	bool exclusive; /* turn off any other lit button LED first (flip-mode) */
};

#if defined(FEATURE_CHESTER_APP_FLIP_MODE)

/* CHESTER Push FM: a press lights that button's LED steady and turns off any
 * other lit button LED (radio-button style); hold is not tracked separately.
 */
#define APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON 1

#define BUTTON_ACTION_PRESS(button_index)                                                         \
	{                                                                                          \
		.event = CTR_Z_EVENT_BUTTON_##button_index##_PRESS,                               \
		.led_channel = CTR_Z_LED_CHANNEL_##button_index##_R,                              \
		.led_param = {CTR_Z_LED_BRIGHTNESS_HIGH, CTR_Z_LED_COMMAND_NONE,                  \
			      CTR_Z_LED_PATTERN_ON},                                              \
		.buzzer_command = CTR_Z_BUZZER_COMMAND_1X_1_2,                                    \
		.is_hold = false,                                                                 \
		.exclusive = true,                                                                \
	}

static const struct app_handler_button_action
	m_button_actions[APP_DATA_BUTTON_COUNT][APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON] = {
		{BUTTON_ACTION_PRESS(0)},
		{BUTTON_ACTION_PRESS(1)},
		{BUTTON_ACTION_PRESS(2)},
		{BUTTON_ACTION_PRESS(3)},
		{BUTTON_ACTION_PRESS(4)},
};

#undef BUTTON_ACTION_PRESS

#else

/* CHESTER Push: click and hold are tracked and signalled independently. */
#define APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON 2

#define BUTTON_ACTION_CLICK(button_index)                                                         \
	{                                                                                          \
		.event = CTR_Z_EVENT_BUTTON_##button_index##_CLICK,                               \
		.led_channel = CTR_Z_LED_CHANNEL_##button_index##_G,                              \
		.led_param = {CTR_Z_LED_BRIGHTNESS_HIGH, CTR_Z_LED_COMMAND_1X_1_2,                \
			      CTR_Z_LED_PATTERN_OFF},                                             \
		.buzzer_command = CTR_Z_BUZZER_COMMAND_1X_1_2,                                    \
		.is_hold = false,                                                                 \
		.exclusive = false,                                                               \
	}

#define BUTTON_ACTION_HOLD(button_index)                                                          \
	{                                                                                          \
		.event = CTR_Z_EVENT_BUTTON_##button_index##_HOLD,                                \
		.led_channel = CTR_Z_LED_CHANNEL_##button_index##_R,                              \
		.led_param = {CTR_Z_LED_BRIGHTNESS_HIGH, CTR_Z_LED_COMMAND_2X_1_2,                \
			      CTR_Z_LED_PATTERN_OFF},                                             \
		.buzzer_command = CTR_Z_BUZZER_COMMAND_2X_1_2,                                    \
		.is_hold = true,                                                                  \
		.exclusive = false,                                                               \
	}

static const struct app_handler_button_action
	m_button_actions[APP_DATA_BUTTON_COUNT][APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON] = {
		{BUTTON_ACTION_CLICK(0), BUTTON_ACTION_HOLD(0)},
		{BUTTON_ACTION_CLICK(1), BUTTON_ACTION_HOLD(1)},
		{BUTTON_ACTION_CLICK(2), BUTTON_ACTION_HOLD(2)},
		{BUTTON_ACTION_CLICK(3), BUTTON_ACTION_HOLD(3)},
		{BUTTON_ACTION_CLICK(4), BUTTON_ACTION_HOLD(4)},
};

#undef BUTTON_ACTION_CLICK
#undef BUTTON_ACTION_HOLD

#endif /* defined(FEATURE_CHESTER_APP_FLIP_MODE) */

/* Turns off the previously lit exclusive-mode LED channel (if any and if
 * different from `channel`), so callers never need to blank all channels. */
static int handler_led_clear_previous(const struct device *dev, enum ctr_z_led_channel channel)
{
	static bool m_lit_valid;
	static enum ctr_z_led_channel m_lit_channel;
	int ret = 0;

	if (m_lit_valid && m_lit_channel != channel) {
		struct ctr_z_led_param led_param = {
			.brightness = CTR_Z_LED_BRIGHTNESS_OFF,
			.command = CTR_Z_LED_COMMAND_NONE,
			.pattern = CTR_Z_LED_PATTERN_OFF,
		};

		ret = ctr_z_set_led(dev, m_lit_channel, &led_param);
		if (ret) {
			LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		}
	}

	m_lit_channel = channel;
	m_lit_valid = true;

	return ret;
}

static int handle_button(const struct device *dev, enum ctr_z_event event,
			  const struct app_handler_button_action *action,
			  struct app_data_button *button)
{
	int ret;

	if (event != action->event) {
		return 0;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (action->exclusive) {
		handler_led_clear_previous(dev, action->led_channel);
	}

	ret = ctr_z_set_led(dev, action->led_channel, &action->led_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		return ret;
	}

	struct ctr_z_buzzer_param buzzer_param = {
		.command = action->buzzer_command,
		.pattern = CTR_Z_BUZZER_PATTERN_OFF,
	};

	ret = ctr_z_set_buzzer(dev, &buzzer_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_buzzer` failed: %d", ret);
		return ret;
	}

	app_data_lock();

	if (button->event_count < APP_DATA_MAX_BUTTON_EVENTS) {
		struct app_data_button_event *event_slot = &button->events[button->event_count];

		ret = ctr_rtc_get_ts(&event_slot->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}

		event_slot->type = action->is_hold ? APP_DATA_BUTTON_EVENT_X_HOLD
						    : APP_DATA_BUTTON_EVENT_X_CLICK;
		button->event_count++;

		LOG_INF("Event count: %d", button->event_count);
	} else {
		LOG_WRN("Event full");
		app_data_unlock();
		return -ENOSPC;
	}

	if (action->is_hold) {
		atomic_inc(&button->hold_count);
	} else {
		atomic_inc(&button->click_count);
	}

	app_data_unlock();

	send_with_rate_limit();

	return 1;
}

void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

	for (int btn_idx = 0; btn_idx < APP_DATA_BUTTON_COUNT; btn_idx++) {
		for (int i = 0; i < APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON; i++) {
			ret = handle_button(dev, event, &m_button_actions[btn_idx][i],
					     &g_app_data.button[btn_idx]);
			if (ret < 0) {
				LOG_ERR("Call `handle_button` failed: %d", ret);
			} else if (ret) {
				goto apply;
			}
		}
	}

	switch (event) {
	case CTR_Z_EVENT_DEVICE_RESET:
		LOG_INF("Event `CTR_Z_EVENT_DEVICE_RESET`");
		goto apply;

	case CTR_Z_EVENT_DC_CONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_CONNECTED`");
		app_work_backup_update();
		handle_dc_event(event);
		break;

	case CTR_Z_EVENT_DC_DISCONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_DISCONNECTED`");
		app_work_backup_update();
		handle_dc_event(event);
		break;
	default:
		break;
	}

apply:
	ret = ctr_z_apply(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
	}
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

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
