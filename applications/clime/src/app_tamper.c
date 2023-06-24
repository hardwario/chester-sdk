/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_edge.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_tamper, LOG_LEVEL_DBG);

#define ON_TIME	 K_MSEC(30)
#define OFF_TIME K_MSEC(1000)

static const struct gpio_dt_spec m_tamper_spec =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), tamper_gpios);

static struct ctr_edge m_edge;

static void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

	struct app_data_tamper *tamper = &g_app_data.tamper;

	app_data_lock();

	tamper->activated = event == CTR_EDGE_EVENT_ACTIVE;

	if (tamper->event_count < APP_DATA_MAX_TAMPER_EVENTS) {
		struct app_data_tamper_event *te = &tamper->events[tamper->event_count];

		ret = ctr_rtc_get_ts(&te->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return;
		}

		te->activated = event == CTR_EDGE_EVENT_ACTIVE;
		tamper->event_count++;

		LOG_INF("Event count: %d", tamper->event_count);
	} else {
		LOG_WRN("Event buffer full");
		app_data_unlock();
		return;
	}

	app_work_send_with_rate_limit();

	app_data_unlock();
}

static struct k_timer m_on_timer;
static struct k_timer m_off_timer;

void off_work_handler(struct k_work *work)
{
	int ret;

	k_timer_start(&m_on_timer, OFF_TIME, K_FOREVER);

	ret = ctr_edge_unwatch(&m_edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_unwatch` failed: %d", ret);
		return;
	}

	if (!device_is_ready(m_tamper_spec.port)) {
		LOG_ERR("Port not ready");
		return;
	}

	ret = gpio_pin_configure_dt(&m_tamper_spec, GPIO_INPUT | GPIO_PULL_DOWN);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return;
	}
}

static K_WORK_DEFINE(m_off_work, off_work_handler);

void off_timer_handler(struct k_timer *timer)
{
	k_work_submit(&m_off_work);
}

void on_work_handler(struct k_work *work)
{
	int ret;

	k_timer_start(&m_off_timer, ON_TIME, K_FOREVER);

	if (!device_is_ready(m_tamper_spec.port)) {
		LOG_ERR("Port not ready");
		return;
	}

	ret = gpio_pin_configure_dt(&m_tamper_spec, GPIO_INPUT | GPIO_PULL_UP);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return;
	}

	ret = ctr_edge_watch(&m_edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		return;
	}
}

static K_WORK_DEFINE(m_on_work, on_work_handler);

void on_timer_handler(struct k_timer *timer)
{
	k_work_submit(&m_on_work);
}

int app_tamper_init(void)
{
	int ret;

	k_timer_init(&m_on_timer, on_timer_handler, NULL);
	k_timer_init(&m_off_timer, off_timer_handler, NULL);

	if (!device_is_ready(m_tamper_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&m_tamper_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_init(&m_edge, &m_tamper_spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(&m_edge, edge_callback, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(&m_edge, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(&m_edge, 20);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(&m_edge, 20);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		return ret;
	}

	k_timer_start(&m_on_timer, K_NO_WAIT, K_FOREVER);

	return 0;
}

int app_tamper_clear(void)
{
	app_data_lock();
	g_app_data.tamper.event_count = 0;
	app_data_unlock();

	return 0;
}
