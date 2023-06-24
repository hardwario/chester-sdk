/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_edge.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(ctr_edge, CONFIG_CTR_EDGE_LOG_LEVEL);

static void event_active_work_handler(struct k_work *work)
{
	struct ctr_edge *edge = CONTAINER_OF(work, struct ctr_edge, event_active_work);

	k_mutex_lock(&edge->lock, K_FOREVER);

	if (edge->cb) {
		edge->cb(edge, CTR_EDGE_EVENT_ACTIVE, edge->user_data);
	}

	k_mutex_unlock(&edge->lock);
}

static void event_inactive_work_handler(struct k_work *work)
{
	struct ctr_edge *edge = CONTAINER_OF(work, struct ctr_edge, event_inactive_work);

	k_mutex_lock(&edge->lock, K_FOREVER);

	if (edge->cb) {
		edge->cb(edge, CTR_EDGE_EVENT_INACTIVE, edge->user_data);
	}

	k_mutex_unlock(&edge->lock);
}

static void event_timer(struct k_timer *timer_id)
{
	int ret;

	struct ctr_edge *edge = CONTAINER_OF(timer_id, struct ctr_edge, event_timer);

	atomic_set(&edge->is_debouncing, false);

	k_timer_stop(&edge->cooldown_timer);

	if (atomic_cas(&edge->is_active, false, true)) {
		ret = gpio_pin_interrupt_configure_dt(edge->spec, GPIO_INT_LEVEL_INACTIVE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_oops();
		}

		k_work_submit(&edge->event_active_work);

	} else if (atomic_cas(&edge->is_active, true, false)) {
		ret = gpio_pin_interrupt_configure_dt(edge->spec, GPIO_INT_LEVEL_ACTIVE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_oops();
		}

		k_work_submit(&edge->event_inactive_work);
	}
}

static void cooldown_timer(struct k_timer *timer_id)
{
	int ret;

	struct ctr_edge *edge = CONTAINER_OF(timer_id, struct ctr_edge, cooldown_timer);

	atomic_set(&edge->is_debouncing, true);

	gpio_flags_t level =
		atomic_get(&edge->is_active) ? GPIO_INT_LEVEL_ACTIVE : GPIO_INT_LEVEL_INACTIVE;

	ret = gpio_pin_interrupt_configure_dt(edge->spec, level);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		k_oops();
	}
}

static void gpio_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	int ret;

	struct ctr_edge *edge = CONTAINER_OF(cb, struct ctr_edge, gpio_cb);

	if (atomic_set(&edge->is_debouncing, false)) {
		k_timer_stop(&edge->event_timer);

		gpio_flags_t level = atomic_get(&edge->is_active) ? GPIO_INT_LEVEL_INACTIVE
								  : GPIO_INT_LEVEL_ACTIVE;

		ret = gpio_pin_interrupt_configure_dt(edge->spec, level);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_oops();
		}

	} else {
		ret = gpio_pin_interrupt_configure_dt(edge->spec, GPIO_INT_DISABLE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_oops();
		}

		k_timeout_t t = K_MSEC(atomic_get(&edge->cooldown_time));
		k_timer_start(&edge->cooldown_timer, t, K_FOREVER);

		if (atomic_get(&edge->is_active)) {
			k_timeout_t t = K_MSEC(atomic_get(&edge->inactive_duration));
			k_timer_start(&edge->event_timer, t, K_FOREVER);

		} else {
			k_timeout_t t = K_MSEC(atomic_get(&edge->active_duration));
			k_timer_start(&edge->event_timer, t, K_FOREVER);
		}
	}
}

int ctr_edge_init(struct ctr_edge *edge, const struct gpio_dt_spec *spec, bool start_active)
{
	int ret;

	memset(edge, 0, sizeof(*edge));

	edge->spec = spec;
	edge->start_active = start_active;

	atomic_set(&edge->is_active, edge->start_active);

	k_mutex_init(&edge->lock);
	k_timer_init(&edge->cooldown_timer, cooldown_timer, NULL);
	k_timer_init(&edge->event_timer, event_timer, NULL);
	k_work_init(&edge->event_active_work, event_active_work_handler);
	k_work_init(&edge->event_inactive_work, event_inactive_work_handler);

	gpio_init_callback(&edge->gpio_cb, gpio_handler, BIT(edge->spec->pin));
	ret = gpio_add_callback(edge->spec->port, &edge->gpio_cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_edge_get_active(struct ctr_edge *edge, bool *is_active)
{
	*is_active = atomic_get(&edge->is_active);

	return 0;
}

int ctr_edge_set_callback(struct ctr_edge *edge, ctr_edge_cb_t cb, void *user_data)
{
	k_mutex_lock(&edge->lock, K_FOREVER);

	edge->cb = cb;
	edge->user_data = user_data;

	k_mutex_unlock(&edge->lock);

	return 0;
}

int ctr_edge_set_cooldown_time(struct ctr_edge *edge, int msec)
{
	if (msec < 0) {
		LOG_ERR("Parameter out of range: %d", msec);
		return -EINVAL;
	}

	atomic_set(&edge->cooldown_time, msec);

	return 0;
}

int ctr_edge_set_active_duration(struct ctr_edge *edge, int msec)
{
	if (msec < 0) {
		LOG_ERR("Parameter out of range: %d", msec);
		return -EINVAL;
	}

	atomic_set(&edge->active_duration, msec);

	return 0;
}

int ctr_edge_set_inactive_duration(struct ctr_edge *edge, int msec)
{
	if (msec < 0) {
		LOG_ERR("Parameter out of range: %d", msec);
		return -EINVAL;
	}

	atomic_set(&edge->inactive_duration, msec);

	return 0;
}

int ctr_edge_watch(struct ctr_edge *edge)
{
	int ret;

	int key = irq_lock();

	gpio_flags_t level =
		atomic_get(&edge->is_active) ? GPIO_INT_LEVEL_INACTIVE : GPIO_INT_LEVEL_ACTIVE;

	ret = gpio_pin_interrupt_configure_dt(edge->spec, level);
	if (ret) {
		irq_unlock(key);
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

	irq_unlock(key);

	return 0;
}

int ctr_edge_unwatch(struct ctr_edge *edge)
{
	int ret;

	int key = irq_lock();

	ret = gpio_pin_interrupt_configure_dt(edge->spec, GPIO_INT_DISABLE);
	if (ret) {
		irq_unlock(key);
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

	atomic_set(&edge->is_debouncing, false);

	k_timer_stop(&edge->cooldown_timer);
	k_timer_stop(&edge->event_timer);

	irq_unlock(key);

	return 0;
}
