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
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_x4.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

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

#if defined(CONFIG_SHIELD_CTR_S1)
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

		atomic_inc(&g_app_data.iaq.press_count);

		struct ctr_s1_led_param param_led = {
			.brightness = CTR_S1_LED_BRIGHTNESS_HIGH,
			.command = CTR_S1_LED_COMMAND_1X_1_1,
			.pattern = CTR_S1_LED_PATTERN_NONE,
		};

		ret = ctr_s1_set_led(dev, CTR_S1_LED_CHANNEL_B, &param_led);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
			k_oops();
		}

		struct ctr_s1_buzzer_param param_buzzer = {
			.command = CTR_S1_BUZZER_COMMAND_1X_1_1,
			.pattern = CTR_S1_BUZZER_PATTERN_NONE,
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

		atomic_inc(&g_app_data.iaq.motion_count);

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
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_Z)

void app_handler_ctr_z(const struct device *dev, enum ctr_z_event backup_event, void *param)
{
	int ret;

	if (backup_event != CTR_Z_EVENT_DC_CONNECTED &&
	    backup_event != CTR_Z_EVENT_DC_DISCONNECTED) {
		return;
	}

	struct app_data_backup *backup = &g_app_data.backup;

	app_work_backup_update();

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
		app_work_send_with_rate_limit();
	}

	if (g_app_config.backup_report_disconnected &&
	    backup_event == CTR_Z_EVENT_DC_DISCONNECTED) {
		app_work_send_with_rate_limit();
	}

	app_data_unlock();
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

#endif /* defined(CONFIG_CTR_BUTTON) */

#if defined(CONFIG_SHIELD_CTR_X4_A) || defined(CONFIG_SHIELD_CTR_X4_B)

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

#endif /* defined(CONFIG_SHIELD_CTR_X4_A) || defined(CONFIG_SHIELD_CTR_X4_B)*/
