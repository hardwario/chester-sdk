/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the raw record ring.
 */

#include "app_driver_store.h"

#include <zephyr/ztest.h>

#include <stdint.h>

#define CAP 4

struct record {
	uint64_t timestamp;
	uint32_t value;
};

static const struct app_driver_demand m_demand = {
	.ctx_size = 12,
	.rec_size = sizeof(struct record),
};

static uint8_t m_mem[256] __aligned(8);
static struct app_driver_slot m_slot;

static void *setup(void)
{
	return NULL;
}

static void before(void *fixture)
{
	struct app_driver_slot_plan plan = {.rec_cap = CAP};

	ARG_UNUSED(fixture);

	plan.bytes = app_driver_slot_bytes(&m_demand, plan.rec_cap, 0, 0);
	zassert_true(plan.bytes <= sizeof(m_mem));

	memset(&m_slot, 0, sizeof(m_slot));
	app_driver_slot_setup(&m_slot, &m_demand, &plan, m_mem, 0);
}

static void push(uint32_t value)
{
	struct record rec = {.timestamp = 1000 + value, .value = value};

	app_driver_rec_push(&m_slot, &rec);
}

static uint32_t value_at(uint32_t seq)
{
	const struct record *rec = app_driver_rec_get(&m_slot, seq);

	zassert_not_null(rec, "seq %u", seq);

	return rec->value;
}

ZTEST(records, test_push_get)
{
	zassert_equal(app_driver_rec_count(&m_slot), 0);
	zassert_is_null(app_driver_rec_get(&m_slot, 0));

	push(10);
	push(11);

	zassert_equal(app_driver_rec_count(&m_slot), 2);
	zassert_equal(app_driver_rec_first(&m_slot), 0);
	zassert_equal(app_driver_rec_end(&m_slot), 2);
	zassert_equal(value_at(0), 10);
	zassert_equal(value_at(1), 11);
	zassert_is_null(app_driver_rec_get(&m_slot, 2));
}

ZTEST(records, test_full_ring_drops_oldest)
{
	for (uint32_t i = 0; i < CAP + 2; i++) {
		push(i);
	}

	zassert_equal(app_driver_rec_count(&m_slot), CAP);
	zassert_equal(app_driver_rec_first(&m_slot), 2);
	zassert_equal(m_slot.drops, 2);
	zassert_is_null(app_driver_rec_get(&m_slot, 1));
	zassert_equal(value_at(2), 2);
	zassert_equal(value_at(CAP + 1), CAP + 1);
}

ZTEST(records, test_pop_until_after_concurrent_push)
{
	push(0);
	push(1);
	push(2);

	/* Sender encodes [first, end) */
	uint32_t end = app_driver_rec_end(&m_slot);

	/* Meanwhile the ring overflows: records 0 and 1 are dropped */
	push(3);
	push(4);
	push(5);

	zassert_equal(app_driver_rec_first(&m_slot), 2);

	/* Only what was encoded is removed; 3, 4 and 5 stay */
	app_driver_rec_pop_until(&m_slot, end);

	zassert_equal(app_driver_rec_first(&m_slot), 3);
	zassert_equal(app_driver_rec_count(&m_slot), 3);
	zassert_equal(value_at(3), 3);
}

ZTEST(records, test_pop_until_bounds)
{
	push(0);
	push(1);

	/* Stale end before tail: no change */
	app_driver_rec_pop_until(&m_slot, 0);
	zassert_equal(app_driver_rec_count(&m_slot), 2);

	/* End beyond head: everything */
	app_driver_rec_pop_until(&m_slot, 100);
	zassert_equal(app_driver_rec_count(&m_slot), 0);
	zassert_equal(app_driver_rec_first(&m_slot), 2);

	/* Sequence numbers continue */
	push(7);
	zassert_equal(value_at(2), 7);
}

ZTEST(records, test_clear)
{
	push(0);
	push(1);

	app_driver_slot_clear(&m_slot);

	zassert_equal(app_driver_rec_count(&m_slot), 0);
	zassert_equal(app_driver_rec_end(&m_slot), 2);
}

ZTEST(records, test_no_capacity)
{
	struct app_driver_slot probe = {0};
	struct app_driver_slot_plan plan = {0};
	uint8_t mem[16] __aligned(8);

	plan.bytes = app_driver_slot_bytes(&m_demand, 0, 0, 0);
	app_driver_slot_setup(&probe, &m_demand, &plan, mem, 0);

	struct record rec = {0};

	app_driver_rec_push(&probe, &rec);

	zassert_equal(app_driver_rec_count(&probe), 0);
	zassert_is_null(app_driver_rec_get(&probe, 0));
	zassert_not_null(probe.ctx);
}

ZTEST_SUITE(records, NULL, setup, before, NULL, NULL);
