/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_EDGE_H_
#define CHESTER_INCLUDE_CTR_EDGE_H_

/* Zephyr includes */
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_edge_event {
	CTR_EDGE_EVENT_INACTIVE = 0,
	CTR_EDGE_EVENT_ACTIVE = 1,
};

struct ctr_edge;

typedef void (*ctr_edge_cb_t)(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data);

struct ctr_edge {
	const struct gpio_dt_spec *spec;
	bool start_active;
	ctr_edge_cb_t cb;
	void *user_data;
	atomic_t cooldown_time;
	atomic_t active_duration;
	atomic_t inactive_duration;
	struct k_mutex lock;
	struct k_timer cooldown_timer;
	struct k_timer event_timer;
	struct k_work event_active_work;
	struct k_work event_inactive_work;
	struct gpio_callback gpio_cb;
	atomic_t is_debouncing;
	atomic_t is_active;
};

int ctr_edge_init(struct ctr_edge *edge, const struct gpio_dt_spec *spec, bool start_active);
int ctr_edge_get_active(struct ctr_edge *edge, bool *is_active);
int ctr_edge_set_callback(struct ctr_edge *edge, ctr_edge_cb_t cb, void *user_data);
int ctr_edge_set_cooldown_time(struct ctr_edge *edge, int msec);
int ctr_edge_set_active_duration(struct ctr_edge *edge, int msec);
int ctr_edge_set_inactive_duration(struct ctr_edge *edge, int msec);
int ctr_edge_watch(struct ctr_edge *edge);
int ctr_edge_unwatch(struct ctr_edge *edge);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_EDGE_H_ */
