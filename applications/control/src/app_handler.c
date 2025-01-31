/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
#include <chester/ctr_led.h>
#include <chester/ctr_rtc.h>
#if defined(FEATURE_HARDWARE_CHESTER_X4_B)
#include <chester/drivers/ctr_x4.h>
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_B) */
#if defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B)
#include <chester/drivers/ctr_x9.h>
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B) */
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
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

static atomic_t m_report_rate_hourly_counter = 0;
static atomic_t m_report_rate_timer_is_active = false;
static atomic_t m_report_delay_timer_is_active = false;

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

#if defined(FEATURE_HARDWARE_CHESTER_X0_A) || defined(FEATURE_HARDWARE_CHESTER_Z)

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

#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

void app_handler_edge_trigger_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
				       void *user_data)
{
	int ret;

	struct app_data_trigger *trigger = CONTAINER_OF(edge, struct app_data_trigger, edge);

	app_data_lock();

	if (trigger->event_count < APP_DATA_MAX_TRIGGER_EVENTS) {
		struct app_data_trigger_event *event = &trigger->events[trigger->event_count];

		ret = ctr_rtc_get_ts(&event->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return;
		}

		event->is_active = edge_event == CTR_EDGE_EVENT_ACTIVE;
		trigger->event_count++;

		LOG_INF("Event count: %d", trigger->event_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return;
	}

	trigger->trigger_active = edge_event == CTR_EDGE_EVENT_ACTIVE;

	LOG_INF("Input: %d, 0x%08x", (int)trigger->trigger_active, (uint32_t)trigger);

	if (g_app_config.trigger_report_active && edge_event == CTR_EDGE_EVENT_ACTIVE) {
		send_with_rate_limit();
	}

	if (g_app_config.trigger_report_inactive && edge_event == CTR_EDGE_EVENT_INACTIVE) {
		send_with_rate_limit();
	}

	app_data_unlock();

	ctr_led_set(CTR_LED_CHANNEL_Y, true);
	k_sleep(K_MSEC(50));
	ctr_led_set(CTR_LED_CHANNEL_Y, false);
}

void app_handler_edge_counter_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
				       void *user_data)
{
	if (edge_event == CTR_EDGE_EVENT_ACTIVE) {
		app_data_lock();

		struct app_data_counter *counter =
			CONTAINER_OF(edge, struct app_data_counter, edge);

		counter->value++;
		counter->delta++;
		LOG_INF("Counter: %llu", counter->value);

		app_data_unlock();
	}
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)

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
		send_with_rate_limit();
	}

	if (g_app_config.backup_report_disconnected &&
	    backup_event == CTR_Z_EVENT_DC_DISCONNECTED) {
		send_with_rate_limit();
	}

	app_data_unlock();
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BUTTON)

static void app_load_timer_handler(struct k_timer *timer)
{
	ctr_led_set(CTR_LED_CHANNEL_LOAD, 0);
}

K_TIMER_DEFINE(app_load_timer, app_load_timer_handler, NULL);

#if defined(FEATURE_HARDWARE_CHESTER_X4_B)
static void enable_x4_outputs(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_b));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return;
	}

	ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, true);
	if (ret) {
		LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, true);
	if (ret) {
		LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, true);
	if (ret) {
		LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, true);
	if (ret) {
		LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
		return;
	}
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_B) */

#if defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B)
static void enable_x9_outputs(void)
{
	int ret;

#if defined(FEATURE_HARDWARE_CHESTER_X9_A)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x9_a));
#elif defined(FEATURE_HARDWARE_CHESTER_X9_B)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x9_b));
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) */

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return;
	}

	ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_1, true);
	if (ret) {
		LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_2, true);
	if (ret) {
		LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_3, true);
	if (ret) {
		LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
		return;
	}

	ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_4, true);
	if (ret) {
		LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
		return;
	}
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B) */

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
			app_work_poll();
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
		case 6:
#if defined(FEATURE_HARDWARE_CHESTER_X4_B)
			enable_x4_outputs();
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_B) */

#if defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B)
			enable_x9_outputs();
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B) */

			break;
		}
	}
}

#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

void app_handler_cloud_event(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param)
{
	if (event != CTR_CLOUD_EVENT_RECV) {
		return;
	}

	LOG_HEXDUMP_INF(data->recv.buf, data->recv.len, "Received:");

	if (data->recv.len < 8) {
		LOG_ERR("Missing encoder hash");
		return;
	}

#if defined(FEATURE_HARDWARE_CHESTER_X4_B) || defined(FEATURE_HARDWARE_CHESTER_X9_A) ||            \
	defined(FEATURE_HARDWARE_CHESTER_X9_B)
	int ret;
	uint8_t *buf = (uint8_t *)data->recv.buf + 8;
	size_t len = data->recv.len - 8;

	ZCBOR_STATE_D(zs, 1, buf, len, 1, 0);

	struct app_cbor_received received;

	ret = app_cbor_decode(zs, &received);
	if (ret) {
		LOG_ERR("Call `app_cbor_decode` failed: %d", ret);
		return;
	}

	LOG_INF("Decode ok");
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_B) || defined(FEATURE_HARDRWARE_CHESTER_X9_A) ||     \
	  defined(FEATURE_HARDWARE_CHESTER_X9_B) */

#if defined(FEATURE_HARDWARE_CHESTER_X4_B)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_b));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return;
	}

	if (received.has_output_1_state) {
		LOG_DBG("Received output_1_state: %d", received.output_1_state);
		if (received.output_1_state < 0 || received.output_1_state > 2) {
			LOG_ERR("Invalid output_1_state value");
			return;
		}
		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, received.output_1_state);
	}

	if (received.has_output_2_state) {
		LOG_DBG("Received output_2_state: %d", received.output_2_state);
		if (received.output_2_state < 0 || received.output_2_state > 2) {
			LOG_ERR("Invalid output_2_state value");
			return;
		}
		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, received.output_2_state);
	}

	if (received.has_output_3_state) {
		LOG_DBG("Received output_3_state: %d", received.output_3_state);
		if (received.output_3_state < 0 || received.output_3_state > 2) {
			LOG_ERR("Invalid output_3_state value");
			return;
		}
		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, received.output_3_state);
	}

	if (received.has_output_4_state) {
		LOG_DBG("Received output_4_state: %d", received.output_4_state);
		if (received.output_4_state < 0 || received.output_4_state > 2) {
			LOG_ERR("Invalid output_3_state value");
			return;
		}
		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, received.output_4_state);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_B) */

#if defined(FEATURE_HARDWARE_CHESTER_X9_A)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x9_a));
#elif defined(FEATURE_HARDWARE_CHESTER_X9_B)
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x9_b));
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) */

#if defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B)
	if (received.has_output_1_state) {
		LOG_DBG("Received output_1_state: %d", received.output_1_state);
		if (received.output_1_state < 0 || received.output_1_state > 2) {
			LOG_ERR("Invalid output_1_state value");
			return;
		}
		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_1, received.output_1_state);
	}

	if (received.has_output_2_state) {
		LOG_DBG("Received output_2_state: %d", received.output_2_state);
		if (received.output_2_state < 0 || received.output_2_state > 2) {
			LOG_ERR("Invalid output_2_state value");
			return;
		}
		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_2, received.output_2_state);
	}

	if (received.has_output_3_state) {
		LOG_DBG("Received output_3_state: %d", received.output_3_state);
		if (received.output_3_state < 0 || received.output_3_state > 2) {
			LOG_ERR("Invalid output_3_state value");
			return;
		}
		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_3, received.output_3_state);
	}

	if (received.has_output_4_state) {
		LOG_DBG("Received output_4_state: %d", received.output_4_state);
		if (received.output_4_state < 0 || received.output_4_state > 2) {
			LOG_ERR("Invalid output_4_state value");
			return;
		}
		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_4, received.output_4_state);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X9_A) || defined(FEATURE_HARDWARE_CHESTER_X9_B) */
}
