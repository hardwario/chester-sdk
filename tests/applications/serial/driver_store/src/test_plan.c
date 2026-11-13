/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the pool plan and the slot memory layout.
 */

#include "app_driver_store.h"

#include <zephyr/ztest.h>

#include <errno.h>
#include <stdint.h>

#define POOL_SIZE (16 * 1024)

/* Sizes of an EM5xx-like legacy driver: private state and one sample record */
struct meter_record {
	uint64_t timestamp;
	float values[20];
};

static const struct app_driver_demand m_meter = {
	.ctx_size = 96,
	.rec_size = sizeof(struct meter_record),
};

enum {
	K_A = 1,
	K_A__MEASUREMENTS,
	K_B,
	K_B__MEASUREMENTS,
	K_C,
	K_C__MEASUREMENTS,
};

static const struct app_driver_item m_items[] = {
	APP_DRIVER_ITEM("a", "", 0, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_STATS, 1000.f,
			K_A),
	APP_DRIVER_ITEM("b", "", 2, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_CIRCULAR, 1000.f,
			K_B),
	APP_DRIVER_ITEM("c", "", 4, APP_MODBUS_S32_HI, 1000.f, APP_DRIVER_AGGREG_COUNTER, 1000.f,
			K_C),
};

static const struct app_driver_demand m_station = {
	.items = m_items,
	.item_count = ARRAY_SIZE(m_items),
};

static const struct app_driver_plan_params m_lte = {
	.interval_sample = 60,
	.interval_aggreg = 300,
	.interval_report = 1800,
	.keep_history = true,
};

static size_t total(const struct app_driver_slot_plan *plans, size_t count)
{
	size_t sum = 0;

	for (size_t i = 0; i < count; i++) {
		sum += plans[i].bytes;
	}

	return sum;
}

ZTEST(plan, test_item_macro)
{
	zassert_equal(m_items[1].key, K_B);
	zassert_equal(m_items[1].key_measurements, K_B__MEASUREMENTS);
	zassert_equal(m_items[1].lrw, APP_DRIVER_LRW_AUTO);
}

ZTEST(plan, test_base_fits_and_grows)
{
	const struct app_driver_demand *demands[] = {&m_meter, NULL, &m_station};
	struct app_driver_slot_plan plans[3];

	int ret = app_driver_pool_plan(demands, 3, &m_lte, POOL_SIZE, plans);

	zassert_ok(ret);
	zassert_true(total(plans, 3) <= POOL_SIZE);

	/* One report period: 1800 / 60 + 1 records, 1800 / 300 + 1 windows, then grown */
	zassert_true(plans[0].rec_cap > 31, "rec_cap %u", plans[0].rec_cap);
	zassert_true(plans[2].win_cap > 7, "win_cap %u", plans[2].win_cap);
	zassert_equal(plans[2].samples_cap, 5);

	/* Unused slot */
	zassert_equal(plans[1].bytes, 0);
	zassert_equal(plans[1].rec_cap, 0);
}

ZTEST(plan, test_growth_is_capped)
{
	static const struct app_driver_demand small = {.rec_size = 16};
	const struct app_driver_demand *demands[] = {&small};
	struct app_driver_slot_plan plan;

	zassert_ok(app_driver_pool_plan(demands, 1, &m_lte, POOL_SIZE, &plan));
	zassert_equal(plan.rec_cap, APP_DRIVER_RING_GROW_MAX);
}

ZTEST(plan, test_no_history_keeps_minimum)
{
	const struct app_driver_plan_params lrw = {
		.interval_sample = 60,
		.interval_aggreg = 300,
		.interval_report = 1800,
		.keep_history = false,
	};
	const struct app_driver_demand *demands[] = {&m_meter, &m_station};
	struct app_driver_slot_plan plans[2];

	zassert_ok(app_driver_pool_plan(demands, 2, &lrw, POOL_SIZE, plans));
	zassert_equal(plans[0].rec_cap, APP_DRIVER_RING_MIN);
	zassert_equal(plans[1].win_cap, APP_DRIVER_RING_MIN);
}

ZTEST(plan, test_fixed_and_unbounded_report)
{
	static const struct app_driver_demand fixed = {.rec_size = 16, .rec_fixed = 32};
	const struct app_driver_plan_params on_demand = {
		.interval_sample = 60,
		.interval_aggreg = 300,
		.interval_report = 0,
		.keep_history = true,
	};
	const struct app_driver_demand *demands[] = {&fixed, &m_meter, &m_station};
	struct app_driver_slot_plan plans[3];

	/* A pool that holds exactly the base plan: no growth */
	size_t base = app_driver_slot_bytes(&fixed, 32, 0, 0) +
		      app_driver_slot_bytes(&m_meter, APP_DRIVER_REC_BASE_MAX / 2, 0, 0) +
		      app_driver_slot_bytes(&m_station, 0, APP_DRIVER_WIN_BASE_MAX, 5);

	zassert_ok(app_driver_pool_plan(demands, 3, &on_demand, base, plans));
	zassert_equal(plans[0].rec_cap, 32);
	zassert_equal(plans[1].rec_cap, APP_DRIVER_REC_BASE_MAX / 2);
	zassert_equal(plans[2].win_cap, APP_DRIVER_WIN_BASE_MAX);
}

ZTEST(plan, test_fixed_records_not_scaled)
{
	static const struct app_driver_demand fixed = {.rec_size = 16, .rec_fixed = 32};
	const struct app_driver_demand *demands[8] = {&fixed};
	struct app_driver_slot_plan plans[8];

	/* Leftover pool does not grow it */
	zassert_ok(app_driver_pool_plan(demands, 1, &m_lte, POOL_SIZE, plans));
	zassert_equal(plans[0].rec_cap, 32);

	/* A tight pool does not shrink it */
	for (int i = 1; i < 8; i++) {
		demands[i] = &m_meter;
	}

	zassert_equal(app_driver_pool_plan(demands, 8, &m_lte, POOL_SIZE, plans), -ENOSPC);
	zassert_equal(plans[0].rec_cap, 32);
	zassert_true(plans[1].rec_cap < 31);
}

ZTEST(plan, test_samples_cap)
{
	const struct app_driver_plan_params fast = {
		.interval_sample = 1,
		.interval_aggreg = 300,
		.interval_report = 1800,
		.keep_history = true,
	};
	static const struct app_driver_item last[] = {
		APP_DRIVER_ITEM("c", "", 4, APP_MODBUS_U16, 1.f, APP_DRIVER_AGGREG_LAST, 1.f, K_C),
	};
	static const struct app_driver_demand no_stats = {.items = last, .item_count = 1};
	const struct app_driver_demand *demands[] = {&m_station, &no_stats};
	struct app_driver_slot_plan plans[2];

	zassert_ok(app_driver_pool_plan(demands, 2, &fast, POOL_SIZE, plans));
	zassert_equal(plans[0].samples_cap, APP_DRIVER_MAX_WINDOW_SAMPLES);
	zassert_equal(plans[1].samples_cap, 0);
}

ZTEST(plan, test_shrink_when_base_does_not_fit)
{
	const struct app_driver_demand *demands[8];
	struct app_driver_slot_plan plans[8];

	for (int i = 0; i < 8; i++) {
		demands[i] = &m_meter;
	}

	int ret = app_driver_pool_plan(demands, 8, &m_lte, POOL_SIZE, plans);

	zassert_equal(ret, -ENOSPC);
	zassert_true(total(plans, 8) <= POOL_SIZE);

	for (int i = 0; i < 8; i++) {
		zassert_true(plans[i].rec_cap >= APP_DRIVER_RING_MIN);
		zassert_true(plans[i].rec_cap < 31, "rec_cap %u", plans[i].rec_cap);
	}
}

ZTEST(plan, test_nomem)
{
	const struct app_driver_demand *demands[] = {&m_meter, &m_meter};
	struct app_driver_slot_plan plans[2];

	zassert_equal(app_driver_pool_plan(demands, 2, &m_lte, 256, plans), -ENOMEM);
}

ZTEST(plan, test_setup_layout_aligned)
{
	static uint8_t pool[POOL_SIZE] __aligned(8);
	const struct app_driver_demand *demands[] = {&m_meter, &m_station};
	struct app_driver_slot_plan plans[2];
	struct app_driver_slot slots[2] = {0};
	uint8_t *p = pool;

	zassert_ok(app_driver_pool_plan(demands, 2, &m_lte, POOL_SIZE, plans));

	for (int i = 0; i < 2; i++) {
		app_driver_slot_setup(&slots[i], demands[i], &plans[i], p, 300);
		p += plans[i].bytes;
		zassert_true(p <= pool + POOL_SIZE);
	}

	zassert_equal((uintptr_t)slots[0].ctx % 8, 0);
	zassert_equal((uintptr_t)slots[0].rec.buf % 8, 0);
	zassert_equal(slots[0].rec.cap, plans[0].rec_cap);
	zassert_is_null(slots[1].ctx);
	zassert_equal((uintptr_t)slots[1].items.state % 8, 0);

	for (size_t i = 0; i < ARRAY_SIZE(m_items); i++) {
		struct app_driver_item_state *st = &slots[1].items.state[i];

		zassert_equal((uintptr_t)st->windows % 8, 0);
		zassert_true(isnan(st->last));

		if (m_items[i].mode == APP_DRIVER_AGGREG_STATS) {
			zassert_equal((uintptr_t)st->samples % 8, 0);
		}
	}

	/* Every record slot is 8-byte aligned too */
	struct meter_record rec = {.timestamp = 1};

	for (int i = 0; i < 3; i++) {
		app_driver_rec_push(&slots[0], &rec);
	}

	zassert_equal((uintptr_t)app_driver_rec_get(&slots[0], 1) % 8, 0);
}

ZTEST_SUITE(plan, NULL, NULL, NULL, NULL, NULL);
