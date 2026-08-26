/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
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
	enum ctr_z_buzzer_command buzzer_command;
	bool is_hold;
};

/* CHESTER Push: click and hold are tracked and signalled independently; the
 * lit LED is turned off later by a per-(button, gesture) timeout (see
 * `m_led_timeouts`), or immediately by a downlink, rather than by a fixed
 * blink pattern. Single-vs-multiple-lit-LED behavior (formerly "Push FM") is
 * a runtime choice, see `g_app_config.led_mode` in `handle_button()`. LED
 * color per button/gesture is configurable at runtime via
 * `g_app_config.button_click_mask`/`button_hold_mask` (see `handle_button()`),
 * so this table only tracks the event/buzzer side of each gesture. */
#define APP_HANDLER_BUTTON_ACTIONS_PER_BUTTON 2

#define BUTTON_ACTION_CLICK(button_index)                                                         \
	{                                                                                          \
		.event = CTR_Z_EVENT_BUTTON_##button_index##_CLICK,                               \
		.buzzer_command = CTR_Z_BUZZER_COMMAND_1X_1_2,                                    \
		.is_hold = false,                                                                 \
	}

#define BUTTON_ACTION_HOLD(button_index)                                                          \
	{                                                                                          \
		.event = CTR_Z_EVENT_BUTTON_##button_index##_HOLD,                                \
		.buzzer_command = CTR_Z_BUZZER_COMMAND_2X_1_2,                                    \
		.is_hold = true,                                                                  \
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

/* R/G/B channel for each button, shared by the physical-press path and the
 * binary LED downlink handler. */
static const enum ctr_z_led_channel m_led_channels[APP_DATA_BUTTON_COUNT][3] = {
	{CTR_Z_LED_CHANNEL_0_R, CTR_Z_LED_CHANNEL_0_G, CTR_Z_LED_CHANNEL_0_B},
	{CTR_Z_LED_CHANNEL_1_R, CTR_Z_LED_CHANNEL_1_G, CTR_Z_LED_CHANNEL_1_B},
	{CTR_Z_LED_CHANNEL_2_R, CTR_Z_LED_CHANNEL_2_G, CTR_Z_LED_CHANNEL_2_B},
	{CTR_Z_LED_CHANNEL_3_R, CTR_Z_LED_CHANNEL_3_G, CTR_Z_LED_CHANNEL_3_B},
	{CTR_Z_LED_CHANNEL_4_R, CTR_Z_LED_CHANNEL_4_G, CTR_Z_LED_CHANNEL_4_B},
};

/* Guards m_led_timeouts[][], m_lit_valid and m_lit_btn_idx below.
 * app_handler_cloud_event() runs on ctr_cloud's own workqueue, genuinely
 * concurrent with handle_button()/led_timeout_work_handler() (both on the
 * system workqueue) -- held across each's full read-decide-cancel-write-zero
 * sequence.
 *
 * Never call k_work_cancel_delayable_sync() while holding this lock:
 * led_timeout_work_handler() below also takes this lock as its first action,
 * so a canceller blocked waiting for an already-running instance of that
 * handler to finish, while that instance is blocked waiting for this same
 * lock, is a guaranteed deadlock. Staleness is instead handled via the
 * generation counter on each `app_handler_led_timeout` -- see
 * handler_arm_led_timeout()/handler_disarm_led_timeout() -- so a plain,
 * non-blocking k_work_cancel_delayable() (best effort only, not required for
 * correctness) is all that's ever needed, from any thread. */
static K_MUTEX_DEFINE(m_led_lock);

struct app_handler_led_timeout {
	struct k_work_delayable work;
	uint8_t btn_idx;
	struct app_config_led_color color; /* what this gesture currently contributes; {0,0,0} = not lit */
	/* `generation` is bumped by every state change (arm or disarm).
	 * `armed_generation` is only updated when a delayable work item is
	 * actually (re)scheduled, snapshotting `generation` as of that schedule.
	 * A fire only acts if the two still match -- if `generation` was bumped
	 * again since (by a disarm, or an arm that left nothing scheduled)
	 * without a fresh schedule following it, `armed_generation` stays behind
	 * and the fire is stale and must no-op. This is what lets cross-thread
	 * callers rely on a plain (non-blocking) cancel instead of waiting for an
	 * in-flight fire to finish. */
	uint32_t generation;
	uint32_t armed_generation;
};

/* [][0] = click timeout, [][1] = hold timeout -- independent slots so
 * setting/cancelling one gesture's timeout never disturbs the other's. Used
 * from `handle_button()` below and from the LED downlink handler
 * (`app_handler_cloud_event()`). */
static struct app_handler_led_timeout m_led_timeouts[APP_DATA_BUTTON_COUNT][2];

/* Writes a button's 3 channels to exactly r/g/b (CHESTER Z takes an
 * arbitrary 8-bit PWM brightness per channel, not just on/off). */
static void handler_set_led_color(const struct device *dev, int btn_idx, uint8_t r, uint8_t g,
				   uint8_t b)
{
	uint8_t values[3] = {r, g, b};

	for (int c = 0; c < 3; c++) {
		struct ctr_z_led_param led_param = {
			.brightness = (enum ctr_z_led_brightness)values[c],
			.command = CTR_Z_LED_COMMAND_NONE,
			.pattern = values[c] ? CTR_Z_LED_PATTERN_ON : CTR_Z_LED_PATTERN_OFF,
		};

		int ret = ctr_z_set_led(dev, m_led_channels[btn_idx][c], &led_param);
		if (ret) {
			LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		}
	}
}

/* Recomputes and writes a button's displayed color as the per-channel max of
 * whatever click and hold currently contribute -- click and hold are
 * independent and can both be lit at once (in multiple mode, or briefly in
 * single mode); only one physical PWM value can drive a shared channel at a
 * time, so the brighter contribution wins per channel rather than picking a
 * winner gesture outright. */
static void handler_update_button_led(const struct device *dev, int btn_idx)
{
	struct app_config_led_color click = m_led_timeouts[btn_idx][0].color;
	struct app_config_led_color hold = m_led_timeouts[btn_idx][1].color;

	handler_set_led_color(dev, btn_idx, MAX(click.r, hold.r), MAX(click.g, hold.g),
			       MAX(click.b, hold.b));
}

static bool m_lit_valid;
static int m_lit_btn_idx;

/* Caller must hold m_led_lock. Arms (or re-arms) a gesture's timeout with a
 * new color: stores the color and bumps `generation`. If `timeout_s` is 0,
 * the LED is left lit with nothing scheduled (0 = stay lit until cleared by
 * a downlink or a different button taking over in single mode) --
 * `armed_generation` is deliberately left behind `generation` in that case,
 * so a stale fire already scheduled by an earlier call can't validate itself
 * against this one. Otherwise the new schedule's generation is snapshotted
 * into `armed_generation`, marking it as the one fire allowed to act.
 * The cancel here is best effort only (non-blocking, safe from any thread):
 * even if an old fire is already running concurrently, its `armed_generation`
 * snapshot from before this call can no longer match `generation`, so it'll
 * no-op regardless of whether the cancel took effect. */
static void handler_arm_led_timeout(struct app_handler_led_timeout *timeout,
				     struct app_config_led_color color, int timeout_s)
{
	timeout->color = color;
	timeout->generation++;

	k_work_cancel_delayable(&timeout->work);
	if (timeout_s > 0) {
		timeout->armed_generation = timeout->generation;
		k_work_schedule(&timeout->work, K_SECONDS(timeout_s));
	}
}

/* Caller must hold m_led_lock. Immediately marks a gesture's slot dark (the
 * hardware write is the caller's job, e.g. via handler_update_button_led())
 * and bumps `generation` without touching `armed_generation`, leaving any
 * fire already scheduled/in-flight for this slot stale so it no-ops --
 * same reasoning as handler_arm_led_timeout(). */
static void handler_disarm_led_timeout(struct app_handler_led_timeout *timeout)
{
	timeout->color = (struct app_config_led_color){0, 0, 0};
	timeout->generation++;

	k_work_cancel_delayable(&timeout->work);
}

/* Caller must hold m_led_lock. Turns off the previously lit button (if any
 * and if different from `btn_idx`), so callers never need to blank every
 * button -- shared by handle_button() and app_handler_cloud_event() so
 * led-mode=single has one shared notion of "what's lit" regardless of which
 * path lit it. Safe to call from either thread: see handler_disarm_led_timeout(). */
static void handler_led_clear_previous_button_locked(const struct device *dev, int btn_idx)
{
	if (m_lit_valid && m_lit_btn_idx != btn_idx) {
		for (int g = 0; g < 2; g++) {
			handler_disarm_led_timeout(&m_led_timeouts[m_lit_btn_idx][g]);
		}
		handler_update_button_led(dev, m_lit_btn_idx);
	}

	m_lit_btn_idx = btn_idx;
	m_lit_valid = true;
}

static void led_timeout_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct app_handler_led_timeout *timeout =
		CONTAINER_OF(dwork, struct app_handler_led_timeout, work);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	k_mutex_lock(&m_led_lock, K_FOREVER);

	bool fired = timeout->generation == timeout->armed_generation;
	if (fired) {
		timeout->color = (struct app_config_led_color){0, 0, 0};
		handler_update_button_led(dev, timeout->btn_idx);
	}
	/* else: superseded by a newer arm/disarm since this fire was scheduled --
	 * stale, no-op */

	k_mutex_unlock(&m_led_lock);

	if (fired) {
		int ret = ctr_z_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
		}
	}
}

static int handler_led_timeout_init(void)
{
	for (int b = 0; b < APP_DATA_BUTTON_COUNT; b++) {
		for (int g = 0; g < 2; g++) {
			m_led_timeouts[b][g].btn_idx = b;
			k_work_init_delayable(&m_led_timeouts[b][g].work, led_timeout_work_handler);
		}
	}

	return 0;
}

SYS_INIT(handler_led_timeout_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int handle_button(const struct device *dev, enum ctr_z_event event,
			  const struct app_handler_button_action *action, int btn_idx,
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

	struct app_config_led_color color = action->is_hold
						     ? g_app_config.button_hold_color[btn_idx]
						     : g_app_config.button_click_color[btn_idx];
	int gesture = action->is_hold ? 1 : 0;

	k_mutex_lock(&m_led_lock, K_FOREVER);

	/* Skip for a zero color (gesture configured with no LED): matches
	 * app_handler_cloud_event()'s same guard -- otherwise a gesture with no
	 * LED of its own would still blank out whatever button is currently lit
	 * without lighting anything in its place. */
	if (g_app_config.led_mode == APP_CONFIG_LED_MODE_SINGLE &&
	    (color.r || color.g || color.b)) {
		handler_led_clear_previous_button_locked(dev, btn_idx);
	}

	handler_arm_led_timeout(&m_led_timeouts[btn_idx][gesture], color, g_app_config.led_timeout);
	handler_update_button_led(dev, btn_idx);

	k_mutex_unlock(&m_led_lock);

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
			ret = handle_button(dev, event, &m_button_actions[btn_idx][i], btn_idx,
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

/* Binary LED downlink: the cloud sets each button's LED color directly
 * (24-bit packed 0xRRGGBB per button; 0 = off), overriding whatever
 * `handle_button()` or a pending `m_led_timeouts` timeout would otherwise
 * do. This also serves as the "visual ACK" for led-timeout=0 buttons that
 * only a downlink can clear. */
void app_handler_cloud_event(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param)
{
	int ret;

	if (event != CTR_CLOUD_EVENT_RECV) {
		return;
	}

	LOG_HEXDUMP_INF(data->recv.buf, data->recv.len, "Received:");

	if (data->recv.len < 8) {
		LOG_ERR("Missing encoder hash");
		return;
	}

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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return;
	}

	const bool has_led_button[APP_DATA_BUTTON_COUNT] = {
		received.has_led_button_0, received.has_led_button_1, received.has_led_button_2,
		received.has_led_button_3, received.has_led_button_4,
	};
	const int32_t led_button[APP_DATA_BUTTON_COUNT] = {
		received.led_button_0, received.led_button_1, received.led_button_2,
		received.led_button_3, received.led_button_4,
	};

	if (g_app_config.led_mode == APP_CONFIG_LED_MODE_SINGLE) {
		int targeted_count = 0;
		for (int btn_idx = 0; btn_idx < APP_DATA_BUTTON_COUNT; btn_idx++) {
			if (has_led_button[btn_idx]) {
				targeted_count++;
			}
		}
		if (targeted_count > 1) {
			LOG_WRN("Downlink targets %d buttons in led-mode=single; only the last "
				"one stays lit",
				targeted_count);
		}
	}

	k_mutex_lock(&m_led_lock, K_FOREVER);

	for (int btn_idx = 0; btn_idx < APP_DATA_BUTTON_COUNT; btn_idx++) {
		if (!has_led_button[btn_idx]) {
			continue;
		}

		/* 24-bit packed color: 0xRRGGBB */
		uint32_t value = (uint32_t)led_button[btn_idx];
		uint8_t r = (value >> 16) & 0xFF;
		uint8_t g = (value >> 8) & 0xFF;
		uint8_t b = value & 0xFF;

		if (g_app_config.led_mode == APP_CONFIG_LED_MODE_SINGLE && (r || g || b)) {
			handler_led_clear_previous_button_locked(dev, btn_idx);
		}

		handler_set_led_color(dev, btn_idx, r, g, b);

		for (int i = 0; i < 2; i++) {
			handler_disarm_led_timeout(&m_led_timeouts[btn_idx][i]);
		}
	}

	k_mutex_unlock(&m_led_lock);

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
