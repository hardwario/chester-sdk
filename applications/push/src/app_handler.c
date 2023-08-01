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

/* Standard includes */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

#if defined(CONFIG_SHIELD_CTR_Z)
static atomic_t m_report_rate_hourly_counter = 0;
static atomic_t m_report_rate_timer_is_active = false;
static atomic_t m_report_delay_timer_is_active = false;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

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

void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
{
	switch (event) {
	case CTR_LTE_EVENT_FAILURE:
		LOG_ERR("Event `CTR_LTE_EVENT_FAILURE`");
		start();
		break;

	case CTR_LTE_EVENT_START_OK:
		LOG_INF("Event `CTR_LTE_EVENT_START_OK`");
		attach();
		break;

	case CTR_LTE_EVENT_START_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_START_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_ATTACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_ATTACH_OK`");
		k_sem_give(&g_app_init_sem);
		break;

	case CTR_LTE_EVENT_ATTACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_ATTACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_DETACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_DETACH_OK`");
		break;

	case CTR_LTE_EVENT_DETACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_DETACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_EVAL_OK:
		LOG_INF("Event `CTR_LTE_EVENT_EVAL_OK`");

		struct ctr_lte_eval *eval = &data->eval_ok.eval;

		LOG_DBG("EEST: %d", eval->eest);
		LOG_DBG("ECL: %d", eval->ecl);
		LOG_DBG("RSRP: %d", eval->rsrp);
		LOG_DBG("RSRQ: %d", eval->rsrq);
		LOG_DBG("SNR: %d", eval->snr);
		LOG_DBG("PLMN: %d", eval->plmn);
		LOG_DBG("CID: %d", eval->cid);
		LOG_DBG("BAND: %d", eval->band);
		LOG_DBG("EARFCN: %d", eval->earfcn);

		k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);
		memcpy(&g_app_data_lte_eval, &data->eval_ok.eval, sizeof(g_app_data_lte_eval));
		g_app_data_lte_eval_valid = true;
		k_mutex_unlock(&g_app_data_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_EVAL_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_EVAL_ERR`");

		k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);
		g_app_data_lte_eval_valid = false;
		k_mutex_unlock(&g_app_data_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LTE_EVENT_SEND_OK`");
		break;

	case CTR_LTE_EVENT_SEND_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_SEND_ERR`");
		start();
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
		return;
	}
}

#if defined(CONFIG_SHIELD_CTR_Z)

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

#if defined(CONFIG_APP_FLIP_MODE)
static int handle_button(enum ctr_z_event event, enum ctr_z_event match,
			 enum ctr_z_led_channel led_channel,
			 enum ctr_z_buzzer_command buzzer_command,
			 enum app_data_button_event_type button_event,
			 struct app_data_button *button)
#else
static int handle_button(enum ctr_z_event event, enum ctr_z_event match,
			 enum ctr_z_led_channel led_channel, enum ctr_z_led_command led_command,
			 enum ctr_z_buzzer_command buzzer_command,
			 enum app_data_button_event_type button_event,
			 struct app_data_button *button)
#endif /* defined(CONFIG_APP_FLIP_MODE) */
{
	int ret;

	if (event != match) {
		return 0;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	struct ctr_z_led_param led_param = {
		.brightness = CTR_Z_LED_BRIGHTNESS_HIGH,
#if defined(CONFIG_APP_FLIP_MODE)
		.command = CTR_Z_LED_COMMAND_NONE,
		.pattern = CTR_Z_LED_PATTERN_ON,
#else
		.command = led_command,
		.pattern = CTR_Z_LED_PATTERN_OFF,
#endif /* defined(CONFIG_APP_FLIP_MODE) */
	};

	ret = ctr_z_set_led(dev, led_channel, &led_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		return ret;
	}

	struct ctr_z_buzzer_param buzzer_param = {
		.command = buzzer_command,
		.pattern = CTR_Z_BUZZER_PATTERN_OFF,
	};

	ret = ctr_z_set_buzzer(dev, &buzzer_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_buzzer` failed: %d", ret);
		return ret;
	}

	bool button_event_type_is_click = true;

	if (event == CTR_Z_EVENT_BUTTON_0_HOLD || event == CTR_Z_EVENT_BUTTON_1_HOLD ||
	    event == CTR_Z_EVENT_BUTTON_2_HOLD || event == CTR_Z_EVENT_BUTTON_3_HOLD ||
	    event == CTR_Z_EVENT_BUTTON_4_HOLD) {
		button_event_type_is_click = false;
	}

	app_data_lock();

	if (button->event_count < APP_DATA_MAX_BUTTON_EVENTS) {
		struct app_data_button_event *event = &button->events[button->event_count];

		ret = ctr_rtc_get_ts(&event->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return ret;
		}

		event->type = button_event_type_is_click ? 0 : 1;
		button->event_count++;

		LOG_INF("Event count: %d", button->event_count);
		LOG_INF("Button event: %d", button_event);
	} else {
		LOG_WRN("Event full");
		app_data_unlock();
		return -ENOSPC;
	}

	if (button_event_type_is_click) {
		atomic_inc(&button->click_count);
	} else {
		atomic_inc(&button->hold_count);
	}

	app_data_unlock();

	send_with_rate_limit();

	return 1;
}

void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

#if defined(CONFIG_APP_FLIP_MODE)

#define CLEAR_LED(button)                                                                          \
	do {                                                                                       \
		struct ctr_z_led_param led_param = {                                               \
			.brightness = CTR_Z_LED_BRIGHTNESS_OFF,                                    \
			.command = CTR_Z_LED_COMMAND_NONE,                                         \
			.pattern = CTR_Z_LED_PATTERN_OFF,                                          \
		};                                                                                 \
		ret = ctr_z_set_led(dev, CTR_Z_LED_CHANNEL_##button##_R, &led_param);              \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);                           \
		}                                                                                  \
	} while (0)

	if (event == CTR_Z_EVENT_BUTTON_0_PRESS || event == CTR_Z_EVENT_BUTTON_1_PRESS ||
	    event == CTR_Z_EVENT_BUTTON_2_PRESS || event == CTR_Z_EVENT_BUTTON_3_PRESS ||
	    event == CTR_Z_EVENT_BUTTON_4_PRESS) {
		CLEAR_LED(0);
		CLEAR_LED(1);
		CLEAR_LED(2);
		CLEAR_LED(3);
		CLEAR_LED(4);
	}

#undef CLEAR_LED

#define HANDLE_PRESS(button_index, button_event)                                                   \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button_index##_PRESS,              \
				    CTR_Z_LED_CHANNEL_##button_index##_R,                          \
				    CTR_Z_BUZZER_COMMAND_1X_1_2, button_event,                     \
				    &g_app_data.button[button_index]);                             \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_PRESS(0, APP_DATA_BUTTON_EVENT_X_CLICK);
	HANDLE_PRESS(1, APP_DATA_BUTTON_EVENT_1_CLICK);
	HANDLE_PRESS(2, APP_DATA_BUTTON_EVENT_2_CLICK);
	HANDLE_PRESS(3, APP_DATA_BUTTON_EVENT_3_CLICK);
	HANDLE_PRESS(4, APP_DATA_BUTTON_EVENT_4_CLICK);

#undef HANDLE_PRESS

#else

#define HANDLE_CLICK(button_index, button_event)                                                   \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button_index##_CLICK,              \
				    CTR_Z_LED_CHANNEL_##button_index##_G,                          \
				    CTR_Z_LED_COMMAND_1X_1_2, CTR_Z_BUZZER_COMMAND_1X_1_2,         \
				    button_event, &g_app_data.button[button_index]);               \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_CLICK(0, APP_DATA_BUTTON_EVENT_X_CLICK);
	HANDLE_CLICK(1, APP_DATA_BUTTON_EVENT_1_CLICK);
	HANDLE_CLICK(2, APP_DATA_BUTTON_EVENT_2_CLICK);
	HANDLE_CLICK(3, APP_DATA_BUTTON_EVENT_3_CLICK);
	HANDLE_CLICK(4, APP_DATA_BUTTON_EVENT_4_CLICK);

#undef HANDLE_CLICK

#define HANDLE_HOLD(button_index, button_event)                                                    \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button_index##_HOLD,               \
				    CTR_Z_LED_CHANNEL_##button_index##_R,                          \
				    CTR_Z_LED_COMMAND_2X_1_2, CTR_Z_BUZZER_COMMAND_2X_1_2,         \
				    button_event, &g_app_data.button[button_index]);               \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_HOLD(0, APP_DATA_BUTTON_EVENT_X_HOLD);
	HANDLE_HOLD(1, APP_DATA_BUTTON_EVENT_1_HOLD);
	HANDLE_HOLD(2, APP_DATA_BUTTON_EVENT_2_HOLD);
	HANDLE_HOLD(3, APP_DATA_BUTTON_EVENT_3_HOLD);
	HANDLE_HOLD(4, APP_DATA_BUTTON_EVENT_4_HOLD);

#undef HANDLE_HOLD

#endif /* defined(CONFIG_APP_FLIP_MODE) */

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

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_CTR_BUTTON)

static void app_load_timer_handler(struct k_timer *timer)
{
	ctr_led_set(CTR_LED_CHANNEL_LOAD, 0);
}

K_TIMER_DEFINE(app_load_timer, app_load_timer_handler, NULL);

void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data)
{
	int ret;

	if (chan != CTR_BUTTON_CHANNEL_INT)
		return;

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

#endif /* defined(CONFIG_CTR_BUTTON) */
