/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_driver_store.h"

/* Zephyr includes */
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Every carved block starts 8-byte aligned (records hold 64-bit timestamps) */
#define BLOCK(_n) ROUND_UP((size_t)(_n), 8)

#define DEG_TO_RAD 0.017453292519943295f
#define RAD_TO_DEG 57.29577951308232f

/* Scale factor of the plan in permille of the base capacity */
#define PLAN_UNIT 1000

static size_t rec_stride(const struct app_driver_demand *demand)
{
	return BLOCK(demand->rec_size);
}

static bool has_stats(const struct app_driver_item *items, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		if (items[i].mode == APP_DRIVER_AGGREG_STATS) {
			return true;
		}
	}

	return false;
}

static uint16_t clamp_cap(int64_t cap, int64_t min, int64_t max)
{
	return (uint16_t)CLAMP(cap, min, max);
}

static int64_t div_ceil(int64_t num, int64_t den)
{
	return (num + den - 1) / den;
}

size_t app_driver_slot_bytes(const struct app_driver_demand *demand, uint16_t rec_cap,
			     uint16_t win_cap, uint16_t samples_cap)
{
	size_t bytes = BLOCK(demand->ctx_size);

	if (demand->rec_size) {
		bytes += BLOCK(rec_stride(demand) * rec_cap);
	}

	if (demand->item_count) {
		bytes += BLOCK(sizeof(struct app_driver_item_state) * demand->item_count);

		for (size_t i = 0; i < demand->item_count; i++) {
			if (demand->items[i].mode == APP_DRIVER_AGGREG_STATS) {
				bytes += BLOCK(sizeof(float) * samples_cap);
				bytes += BLOCK(sizeof(struct ctr_data_aggreg) * win_cap);
			} else {
				bytes += BLOCK(sizeof(float) * win_cap);
			}
		}
	}

	return bytes;
}

static void base_plan(const struct app_driver_demand *demand,
		      const struct app_driver_plan_params *params,
		      struct app_driver_slot_plan *plan)
{
	memset(plan, 0, sizeof(*plan));

	if (demand->rec_size) {
		if (!params->keep_history) {
			plan->rec_cap = APP_DRIVER_RING_MIN;
		} else if (demand->rec_fixed) {
			plan->rec_cap = demand->rec_fixed;
		} else if (params->interval_report > 0 && params->interval_sample > 0) {
			plan->rec_cap = clamp_cap(
				div_ceil(params->interval_report, params->interval_sample) + 1,
				APP_DRIVER_RING_MIN, APP_DRIVER_REC_BASE_MAX);
		} else {
			plan->rec_cap = APP_DRIVER_REC_BASE_MAX / 2;
		}
	}

	if (demand->item_count) {
		if (!params->keep_history) {
			plan->win_cap = APP_DRIVER_RING_MIN;
		} else if (params->interval_report > 0 && params->interval_aggreg > 0) {
			plan->win_cap = clamp_cap(
				div_ceil(params->interval_report, params->interval_aggreg) + 1,
				APP_DRIVER_RING_MIN, APP_DRIVER_WIN_BASE_MAX);
		} else {
			plan->win_cap = APP_DRIVER_WIN_BASE_MAX;
		}

		if (has_stats(demand->items, demand->item_count)) {
			int64_t n = 1;

			if (params->interval_sample > 0) {
				n = div_ceil(params->interval_aggreg, params->interval_sample);
			}

			plan->samples_cap = clamp_cap(n, 1, APP_DRIVER_MAX_WINDOW_SAMPLES);
		}
	}
}

/* Ring capacity scaled to factor (permille of base), within the allowed range */
static uint16_t scale_cap(uint16_t base, int64_t factor)
{
	if (!base) {
		return 0;
	}

	return clamp_cap((int64_t)base * factor / PLAN_UNIT, APP_DRIVER_RING_MIN,
			 MAX(base, APP_DRIVER_RING_GROW_MAX));
}

static size_t plan_total(const struct app_driver_demand *const *demands, size_t count,
			 const struct app_driver_slot_plan *base, int64_t factor,
			 struct app_driver_slot_plan *out)
{
	size_t total = 0;

	for (size_t i = 0; i < count; i++) {
		struct app_driver_slot_plan p = {0};

		if (demands[i]) {
			/* A fixed record capacity is not scaled */
			p.rec_cap = demands[i]->rec_fixed ? base[i].rec_cap
							  : scale_cap(base[i].rec_cap, factor);
			p.win_cap = scale_cap(base[i].win_cap, factor);
			p.samples_cap = base[i].samples_cap;
			p.bytes = app_driver_slot_bytes(demands[i], p.rec_cap, p.win_cap,
							p.samples_cap);
		}

		if (out) {
			out[i] = p;
		}

		total += p.bytes;
	}

	return total;
}

/* Largest factor in [lo, hi] whose plan fits the pool (lo must fit) */
static int64_t search_factor(const struct app_driver_demand *const *demands, size_t count,
			     const struct app_driver_slot_plan *base, size_t pool_size, int64_t lo,
			     int64_t hi)
{
	while (lo < hi) {
		int64_t mid = lo + (hi - lo + 1) / 2;

		if (plan_total(demands, count, base, mid, NULL) <= pool_size) {
			lo = mid;
		} else {
			hi = mid - 1;
		}
	}

	return lo;
}

int app_driver_pool_plan(const struct app_driver_demand *const *demands, size_t count,
			 const struct app_driver_plan_params *params, size_t pool_size,
			 struct app_driver_slot_plan *plans)
{
	struct app_driver_slot_plan base[count];
	int64_t factor = PLAN_UNIT;
	int ret = 0;

	for (size_t i = 0; i < count; i++) {
		if (demands[i]) {
			base_plan(demands[i], params, &base[i]);
		} else {
			memset(&base[i], 0, sizeof(base[i]));
		}
	}

	if (plan_total(demands, count, base, PLAN_UNIT, NULL) <= pool_size) {
		if (params->keep_history) {
			factor = search_factor(demands, count, base, pool_size, PLAN_UNIT,
					       (int64_t)PLAN_UNIT * APP_DRIVER_RING_GROW_MAX);
		}
	} else if (plan_total(demands, count, base, 0, NULL) <= pool_size) {
		factor = search_factor(demands, count, base, pool_size, 0, PLAN_UNIT - 1);
		ret = -ENOSPC;
	} else {
		factor = 0;
		ret = -ENOMEM;
	}

	plan_total(demands, count, base, factor, plans);

	return ret;
}

void app_driver_slot_setup(struct app_driver_slot *slot, const struct app_driver_demand *demand,
			   const struct app_driver_slot_plan *plan, void *mem, int interval)
{
	uint8_t *p = mem;

	memset(mem, 0, plan->bytes);

	slot->ctx = demand->ctx_size ? p : NULL;
	p += BLOCK(demand->ctx_size);

	memset(&slot->rec, 0, sizeof(slot->rec));

	if (demand->rec_size) {
		slot->rec.buf = p;
		slot->rec.size = demand->rec_size;
		slot->rec.cap = plan->rec_cap;
		p += BLOCK(rec_stride(demand) * plan->rec_cap);
	}

	memset(&slot->items, 0, sizeof(slot->items));

	if (demand->item_count) {
		struct app_driver_items *items = &slot->items;

		items->desc = demand->items;
		items->count = demand->item_count;
		items->samples_cap = plan->samples_cap;
		items->win_cap = plan->win_cap;
		items->interval = interval;

		items->state = (struct app_driver_item_state *)p;
		p += BLOCK(sizeof(struct app_driver_item_state) * demand->item_count);

		for (size_t i = 0; i < demand->item_count; i++) {
			struct app_driver_item_state *st = &items->state[i];

			st->last = NAN;

			if (demand->items[i].mode == APP_DRIVER_AGGREG_STATS) {
				st->samples = (float *)p;
				p += BLOCK(sizeof(float) * plan->samples_cap);
				st->windows = p;
				p += BLOCK(sizeof(struct ctr_data_aggreg) * plan->win_cap);
			} else {
				st->windows = p;
				p += BLOCK(sizeof(float) * plan->win_cap);
			}
		}
	}

	slot->drops = 0;

	__ASSERT_NO_MSG((size_t)(p - (uint8_t *)mem) == plan->bytes);
}

void app_driver_rec_push(struct app_driver_slot *slot, const void *rec)
{
	struct app_driver_records *r = &slot->rec;

	if (!r->cap) {
		return;
	}

	if (r->head - r->tail >= r->cap) {
		r->tail++;
		slot->drops++;
	}

	memcpy(r->buf + (r->head % r->cap) * BLOCK(r->size), rec, r->size);
	r->head++;
}

const void *app_driver_rec_get(const struct app_driver_slot *slot, uint32_t seq)
{
	const struct app_driver_records *r = &slot->rec;

	if (!r->cap || seq - r->tail >= r->head - r->tail) {
		return NULL;
	}

	return r->buf + (seq % r->cap) * BLOCK(r->size);
}

static void pop_until(uint32_t *tail, uint32_t head, uint32_t end)
{
	if ((int32_t)(end - *tail) <= 0) {
		return;
	}

	*tail = (int32_t)(end - head) > 0 ? head : end;
}

void app_driver_rec_pop_until(struct app_driver_slot *slot, uint32_t end)
{
	pop_until(&slot->rec.tail, slot->rec.head, end);
}

void app_driver_items_put(struct app_driver_slot *slot, size_t idx, float value)
{
	struct app_driver_items *items = &slot->items;

	if (idx >= items->count || isnan(value)) {
		return;
	}

	struct app_driver_item_state *st = &items->state[idx];

	st->last = value;

	if (st->count == UINT16_MAX) {
		return;
	}

	switch (items->desc[idx].mode) {
	case APP_DRIVER_AGGREG_STATS:
		if (!st->count || value < st->min) {
			st->min = value;
		}
		if (!st->count || value > st->max) {
			st->max = value;
		}
		st->sum += value;
		if (st->count < items->samples_cap) {
			st->samples[st->count] = value;
		}
		break;

	case APP_DRIVER_AGGREG_CIRCULAR:
		st->sum_sin += sinf(value * DEG_TO_RAD);
		st->sum_cos += cosf(value * DEG_TO_RAD);
		break;

	default:
		break;
	}

	st->count++;
}

float app_driver_items_last(const struct app_driver_slot *slot, size_t idx)
{
	if (idx >= slot->items.count) {
		return NAN;
	}

	return slot->items.state[idx].last;
}

static int compare(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;

	return (fa > fb) - (fa < fb);
}

static float circular_mean(const struct app_driver_item_state *st)
{
	/* Mean resultant length close to 0: the directions cancel out, no mean direction */
	if (hypotf(st->sum_sin, st->sum_cos) < 1e-3f * st->count) {
		return NAN;
	}

	float deg = atan2f(st->sum_sin, st->sum_cos) * RAD_TO_DEG;

	if (deg < 0.f) {
		deg += 360.f;
	}

	return deg >= 360.f ? 0.f : deg;
}

static void close_window(struct app_driver_slot *slot, bool empty)
{
	struct app_driver_items *items = &slot->items;

	if (items->win_head - items->win_tail >= items->win_cap) {
		items->win_tail++;
		slot->drops++;
	}

	uint32_t pos = items->win_head % items->win_cap;

	for (size_t i = 0; i < items->count; i++) {
		struct app_driver_item_state *st = &items->state[i];
		bool none = empty || !st->count;

		if (items->desc[i].mode == APP_DRIVER_AGGREG_STATS) {
			struct ctr_data_aggreg *w = &((struct ctr_data_aggreg *)st->windows)[pos];

			if (none) {
				w->min = w->max = w->avg = w->mdn = NAN;
				continue;
			}

			size_t n = MIN(st->count, items->samples_cap);

			qsort(st->samples, n, sizeof(float), compare);

			w->min = st->min;
			w->max = st->max;
			w->avg = st->sum / st->count;
			w->mdn = n ? st->samples[n / 2] : NAN;
		} else {
			float *w = &((float *)st->windows)[pos];

			if (none) {
				*w = NAN;
			} else if (items->desc[i].mode == APP_DRIVER_AGGREG_CIRCULAR) {
				*w = circular_mean(st);
			} else {
				*w = st->last;
			}
		}
	}

	items->win_head++;
}

void app_driver_items_aggreg(struct app_driver_slot *slot, int64_t ts, int64_t uptime,
			     uint32_t ticks)
{
	struct app_driver_items *items = &slot->items;

	/* No tick: the open window stays open */
	if (!items->count || !ticks) {
		return;
	}

	if (items->win_cap) {
		/* An empty ring anchors the timeline at this window */
		bool anchor = items->win_head == items->win_tail;

		if (!anchor && items->interval > 0) {
			/* Ticks that coalesced while the work queue was blocked; a late tick is
			 * not a missed one
			 */
			uint32_t missed = ticks - 1;
			int64_t interval = (int64_t)items->interval * 1000;
			int64_t elapsed = uptime - items->last_close_uptime;
			/* RTC has a resolution of 1 s */
			int64_t step = (ts - items->last_close_ts) * 1000 - elapsed;

			if (missed >= items->win_cap) {
				/* No window in the ring would survive the empty ones: restart */
				slot->drops += items->win_head - items->win_tail;
				items->win_tail = items->win_head;
				anchor = true;
			} else {
				for (uint32_t k = 0; k < missed; k++) {
					close_window(slot, true);
				}

				/* The clock was set: keep the windows, re-anchor the timeline */
				if (step > interval / 2 + 1000 || step < -(interval / 2 + 1000)) {
					anchor = true;
				}
			}
		}

		close_window(slot, false);

		if (anchor) {
			items->base_ts = ts - (int64_t)(items->win_head - 1) * items->interval;
		}
	}

	for (size_t i = 0; i < items->count; i++) {
		struct app_driver_item_state *st = &items->state[i];

		st->count = 0;
		st->sum = 0.f;
		st->sum_sin = 0.f;
		st->sum_cos = 0.f;
	}

	items->last_close_ts = ts;
	items->last_close_uptime = uptime;
}

static bool win_valid(const struct app_driver_items *items, size_t idx, uint32_t seq)
{
	return idx < items->count && items->win_cap &&
	       seq - items->win_tail < items->win_head - items->win_tail;
}

const struct ctr_data_aggreg *app_driver_win_stats(const struct app_driver_slot *slot, size_t idx,
						   uint32_t seq)
{
	const struct app_driver_items *items = &slot->items;

	if (!win_valid(items, idx, seq) || items->desc[idx].mode != APP_DRIVER_AGGREG_STATS) {
		return NULL;
	}

	return &((const struct ctr_data_aggreg *)items->state[idx].windows)[seq % items->win_cap];
}

float app_driver_win_value(const struct app_driver_slot *slot, size_t idx, uint32_t seq)
{
	const struct app_driver_items *items = &slot->items;

	if (!win_valid(items, idx, seq)) {
		return NAN;
	}

	if (items->desc[idx].mode == APP_DRIVER_AGGREG_STATS) {
		return app_driver_win_stats(slot, idx, seq)->avg;
	}

	return ((const float *)items->state[idx].windows)[seq % items->win_cap];
}

void app_driver_win_pop_until(struct app_driver_slot *slot, uint32_t end)
{
	pop_until(&slot->items.win_tail, slot->items.win_head, end);
}

void app_driver_slot_clear(struct app_driver_slot *slot)
{
	slot->rec.tail = slot->rec.head;
	slot->items.win_tail = slot->items.win_head;
}

void app_driver_aggreg_samples(float *samples, size_t count, struct ctr_data_aggreg *out)
{
	out->min = NAN;
	out->max = NAN;
	out->avg = NAN;
	out->mdn = NAN;

	if (!count) {
		return;
	}

	for (size_t i = 0; i < count; i++) {
		if (isnan(samples[i])) {
			return;
		}
	}

	qsort(samples, count, sizeof(float), compare);

	out->min = samples[0];
	out->max = samples[count - 1];

	double avg = 0;
	for (size_t i = 0; i < count; i++) {
		avg += (double)samples[i];
	}
	avg /= count;

	out->avg = avg;
	out->mdn = samples[count / 2];
}

bool app_driver_value_to_int32(float value, float mul, int32_t *out)
{
	float v = value * mul;

	/* -2^31 is exact; the largest float below 2^31 rounds into the int32 range */
	if (isnan(v) || v >= 2147483648.f || v < -2147483648.f) {
		return false;
	}

	*out = (int32_t)lroundf(v);

	return true;
}
