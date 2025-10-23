/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_init.h"
#include "app_power.h"
#include "app_send.h"
#include "app_shell.h"
#include "app_sensor.h"
#include "app_work.h"
#include "wmbus.h"
#include "packet.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

/* CHESTER includes */
#include <chester/ctr_rtc.h>
#include <chester/ctr_cloud.h>

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

static bool scheduler_check(void)
{
	static bool previous = false;
	static int64_t interval_last_ts = 0;

	int64_t current_ts;
	ctr_rtc_get_ts(&current_ts);

	struct ctr_rtc_tm tm;
	ctr_rtc_get_tm(&tm);

	// LOG_INF("Scheduler check");

	switch (g_app_config.scan_mode) {
	case APP_CONFIG_SCAN_MODE_OFF:
		return false;
		break;

	case APP_CONFIG_SCAN_MODE_INTERVAL:
		/*LOG_WRN("current_ts: %lld, next: %lld", current_ts,
			interval_last_ts + g_app_config.scan_interval);*/
		if (current_ts > interval_last_ts + g_app_config.scan_interval) {
			// LOG_ERR("OK:)");
			interval_last_ts = current_ts;
			return true;
		}
		break;

	case APP_CONFIG_SCAN_MODE_DAILY:
		if (tm.hours == g_app_config.scan_hour && tm.minutes == 0) {
			if (previous) {
				return false;
			} else {
				previous = true;
				return true;
			}
		} else {
			previous = false;
			return false;
		}
		break;

	case APP_CONFIG_SCAN_MODE_WEEKLY:
		if (tm.wday == g_app_config.scan_weekday && tm.hours == g_app_config.scan_hour &&
		    tm.minutes == 0) {
			if (previous) {
				return false;
			} else {
				previous = true;
				return true;
			}
		} else {
			previous = false;
			return false;
		}
		break;

	case APP_CONFIG_SCAN_MODE_MONTHLY:
		if (tm.day == g_app_config.scan_day && tm.hours == g_app_config.scan_hour &&
		    tm.minutes == 0) {
			if (previous) {
				return false;
			} else {
				previous = true;
				return true;
			}
		} else {
			previous = false;
			return false;
		}
		break;
	default:
		break;
	}

	return 0;
}

static struct k_timer m_scan_timeout_timer;

static void second_antenna_work_handler(struct k_work *work)
{
	int ret;

	LOG_INF("Antenna set: 2");

	atomic_set(&g_app_data.antenna_dual, false);

	k_timer_stop(&m_scan_timeout_timer);

	// Restart timeout timer
	if (!g_app_data.enroll_mode) {
		LOG_INF("Scan timeout %d s", g_app_config.scan_timeout);
		k_timer_start(&m_scan_timeout_timer, K_SECONDS(g_app_config.scan_timeout),
			      K_FOREVER);
	} else {
		LOG_INF("Enroll mode timeout %d s", g_app_data.enroll_timeout);
		k_timer_start(&m_scan_timeout_timer, K_SECONDS(g_app_data.enroll_timeout),
			      K_FOREVER);
	}

	ret = wmbus_antenna_set(2);
	if (ret) {
		LOG_ERR("Call `wmbus_antenna_set` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_second_antenna_work, second_antenna_work_handler);

void app_work_scan_timeout(void)
{
	int ret;

	if (g_app_data.antenna_dual) {
		// Dual antenna scan, switch antennas
		ret = k_work_submit_to_queue(&m_work_q, &m_second_antenna_work);
		if (ret < 0) {
			LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
		}

		struct shell *shell = app_shell_get();
		if (shell) {
			shell_print(shell, "Switched antenna");
		}
	} else {
		/* Stop timeout timer in case scan_timeout was triggered manually from shell */
		k_timer_stop(&m_scan_timeout_timer);
		g_app_data.scan_stop_timestamp = k_uptime_get();

		if (g_app_data.enroll_mode) {

			struct shell *shell = app_shell_get();
			if (shell) {
				size_t device_count;
				wmbus_get_config_device_count(&device_count);
				shell_print(shell, "Enrolled devices: %d", device_count);
				shell_print(shell, "Sending data, restarting...");
			}
		}

		app_work_send_trigger();
	}
}

static void scan_timeout_timer_handler(struct k_timer *timer_id)
{
	app_work_scan_timeout();
}
static K_TIMER_DEFINE(m_scan_timeout_timer, scan_timeout_timer_handler, NULL);

/* Work scan trigger */

static void scan_trigger_work_handler(struct k_work *work)
{
	int ret;

	if (atomic_get(&g_app_data.working_flag) == false) {
		atomic_set(&g_app_data.working_flag, true);
		atomic_set(&g_app_data.send_index, false);
		atomic_inc(&g_app_data.scan_transaction);

		/* If dual antenna is configured, set the flag for second scan with second antenna
		 */
		atomic_set(&g_app_data.antenna_dual, g_app_config.scan_ant == 1 ? true : false);

		g_app_data.scan_start_timestamp = k_uptime_get();
		LOG_INF("Start scan");

		packet_clear();
		wmbus_clear_address_flags();

		if (!g_app_data.enroll_mode) {
			LOG_INF("Scan timeout %d s", g_app_config.scan_timeout);
			k_timer_start(&m_scan_timeout_timer, K_SECONDS(g_app_config.scan_timeout),
				      K_FOREVER);
		} else {
			LOG_INF("Enroll mode timeout %d s", g_app_data.enroll_timeout);
			k_timer_start(&m_scan_timeout_timer, K_SECONDS(g_app_data.enroll_timeout),
				      K_FOREVER);
		}

		LOG_INF("Antenna set: 1");
		ret = wmbus_antenna_set(1);
		if (ret) {
			LOG_ERR("Call `wmbus_antenna_set` failed: %d", ret);
		}

		ret = wmbus_enable();
		if (ret) {
			LOG_ERR("Call `wmbus_enable` failed: %d", ret);
		}

	} else {
		LOG_WRN("Scan already in progress");
	}
}

static K_WORK_DEFINE(m_scan_trigger_work, scan_trigger_work_handler);

void app_work_scan_trigger(void)
{
	int ret;

	g_app_data.enroll_mode = false;

	ret = k_work_submit_to_queue(&m_work_q, &m_scan_trigger_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

void app_work_scan_trigger_enroll(int timeout, int rssi_threshold)
{
	int ret;

	g_app_data.enroll_mode = true;
	g_app_data.enroll_timeout = timeout;
	g_app_data.enroll_rssi_threshold = rssi_threshold;

	// Force to scan all during teach-mode
	g_app_data.scan_all = true;

	app_config_clear_address();

	ret = k_work_submit_to_queue(&m_work_q, &m_scan_trigger_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

/* Work send trigger */

static void send_trigger_work_handler(struct k_work *work)
{
	int ret;

	k_timer_stop(&m_scan_timeout_timer);

	ret = wmbus_disable();
	if (ret) {
		LOG_ERR("Call `wmbus_disable` failed: %d", ret);
	}

	// Disable CHESTER-B1 RF switch
	LOG_INF("Antenna set: 0");
	ret = wmbus_antenna_set(0);
	if (ret) {
		LOG_ERR("Call `wmbus_antenna_set` failed: %d", ret);
	}

	ret = app_send();
	if (ret) {
		LOG_ERR("Call `app_send` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_send_trigger_work, send_trigger_work_handler);

void app_work_send_trigger(void)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_send_trigger_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

/* Cloud poll trigger */

static void poll_trigger_work_handler(struct k_work *work)
{
#if defined(CONFIG_SHIELD_CTR_LTE_V2)

	int ret;

	ret = ctr_cloud_poll_immediately();
	if (ret) {
		LOG_ERR("Call `ctr_cloud_poll_immediately` failed: %d", ret);
	}

#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */
}

static K_WORK_DEFINE(m_poll_trigger_work, poll_trigger_work_handler);

void app_work_poll_trigger(void)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_poll_trigger_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

/* Scheduler loop */

int scheduler_loop(void)
{
	int ret;

	struct ctr_rtc_tm t;
	ret = ctr_rtc_get_tm(&t);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_tm` failed: %d", ret);
		return ret;
	}

	if (scheduler_check()) {
		app_work_scan_trigger();
	}

	return 0;
}

/*
	Scheduler work and timer
	========================
*/

static void scheduler_loop_work_handler(struct k_work *work)
{
	int ret;

	ret = scheduler_loop();
	if (ret) {
		LOG_ERR("Call `scheduler_loop` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_scheduler_loop_work, scheduler_loop_work_handler);

static void scheduler_loop_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_scheduler_loop_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_scheduler_loop_timer, scheduler_loop_timer_handler, NULL);

static void sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_MINUTES(10));

	ret = app_sensor_sample();
	if (ret < 0) {
		LOG_ERR("Call `app_sensor_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_sample_work, sample_work_handler);

#if defined(FEATURE_SUBSYSTEM_BATT)

static void power_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_HOURS(12));

	ret = app_power_sample();
	if (ret < 0) {
		LOG_ERR("Call `app_power_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_power_work, power_work_handler);

#endif /* defined(FEATURE_SUBSYSTEM_BATT) */

int app_work_init(void)
{
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);
	k_thread_name_set(&m_work_q.thread, "app_work");

	k_work_schedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
#if defined(FEATURE_SUBSYSTEM_BATT)
	k_work_schedule_for_queue(&m_work_q, &m_power_work, K_SECONDS(60));
#endif /* defined(FEATURE_SUBSYSTEM_BATT) */

	k_timer_start(&m_scheduler_loop_timer, K_NO_WAIT, K_SECONDS(1));

	return 0;
}
