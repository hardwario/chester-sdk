/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_device.h"
#include "app_driver.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_work.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

/* Forward declarations */
static void sample_timer_handler(struct k_timer *timer);
static void send_timer_handler(struct k_timer *timer);
static void device_aggreg_timer_handler(struct k_timer *timer);
static void sample_work_handler(struct k_work *work);
static void send_work_handler(struct k_work *work);

/* Work queue */
static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

/* Work items */
static K_WORK_DEFINE(m_sample_work, sample_work_handler);
static K_WORK_DEFINE(m_send_work, send_work_handler);

/* Timers */
static K_TIMER_DEFINE(m_sample_timer, sample_timer_handler, NULL);
static K_TIMER_DEFINE(m_send_timer, send_timer_handler, NULL);
static K_TIMER_DEFINE(m_device_aggreg_timer, device_aggreg_timer_handler, NULL);

/* Aggregation windows of the table-driven devices (interval-aggreg) */
static void device_aggreg_work_handler(struct k_work *work)
{
	/* Expirations since the last run: more than one if the work queue was blocked longer
	 * than an interval (the submissions coalesced), none if the timer resubmitted the work
	 * while it ran and that run already counted the expiration
	 */
	uint32_t ticks = k_timer_status_get(&m_device_aggreg_timer);

	if (!ticks) {
		return;
	}

	if (ticks > 1) {
		LOG_WRN("Aggregation delayed, %u windows missed", ticks - 1);
	}

	app_device_aggreg_all(ticks);
}

static K_WORK_DEFINE(m_device_aggreg_work, device_aggreg_work_handler);

static void device_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_device_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

static void ble_tag_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_ble_tag_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_ble_tag_sample_work, ble_tag_sample_work_handler);

static void ble_tag_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_ble_tag_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_ble_tag_sample_timer, ble_tag_sample_timer_handler, NULL);

static void ble_tag_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_ble_tag_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_ble_tag_aggreg_work, ble_tag_aggreg_work_handler);

static void ble_tag_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_ble_tag_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_ble_tag_aggreg_timer, ble_tag_aggreg_timer_handler, NULL);

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

int app_work_init(void)
{
	LOG_INF("Initializing work queue");

	/* Start work queue */
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	/* Set thread name for debugging */
	k_thread_name_set(&m_work_q.thread, "app_work");

	/* Sample timer: 10s initial delay (MicroSENS warmup), then interval */
	k_timer_start(&m_sample_timer, K_SECONDS(10), K_SECONDS(g_app_config.interval_sample));

	/* Send timer: one-shot, 30s initial delay */
	k_timer_start(&m_send_timer, K_SECONDS(30), K_FOREVER);

	k_timer_start(&m_device_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	k_timer_start(&m_ble_tag_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_ble_tag_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

	LOG_INF("Work queue initialized");

	return 0;
}

void app_work_sample(void)
{
	k_timer_start(&m_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
}

void app_work_send(void)
{
	k_timer_start(&m_send_timer, K_NO_WAIT, K_FOREVER);
}

static void sample_timer_handler(struct k_timer *timer)
{
	k_work_submit_to_queue(&m_work_q, &m_sample_work);
}

static void send_timer_handler(struct k_timer *timer)
{
	k_work_submit_to_queue(&m_work_q, &m_send_work);
}

static void sample_work_handler(struct k_work *work)
{
	int ret;

	LOG_INF("Sampling sensors");

	ret = app_sensor_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_sample` failed: %d", ret);
	}

	ret = app_driver_sample();
	if (ret) {
		LOG_ERR("Call `app_driver_sample` failed: %d", ret);
	}

	LOG_INF("Sampling complete");
}

static void send_work_handler(struct k_work *work)
{
	LOG_INF("Sending data");

	/* Restart timer for next send (skip if event-driven only) */
	if (g_app_config.interval_report > 0) {
		k_timer_start(&m_send_timer, K_SECONDS(g_app_config.interval_report), K_FOREVER);
	}

	/* app_send() drops the reported data (device buffers and BLE tag measurements), sent or not */
	int ret = app_send();
	if (ret) {
		LOG_ERR("Call `app_send` failed: %d", ret);
		return;
	}

	LOG_INF("Send complete");
}
