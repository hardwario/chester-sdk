/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the item store: the four aggregation modes, the window ring and its
 * timeline.
 */

#include "app_driver_store.h"

#include <zephyr/ztest.h>

#include <math.h>
#include <stdint.h>

#define INTERVAL    300
#define WIN_CAP     4
#define SAMPLES_CAP 4
#define T0          1700000000

enum {
	K_STATS = 1,
	K_STATS__MEASUREMENTS,
	K_LAST,
	K_LAST__MEASUREMENTS,
	K_COUNTER,
	K_COUNTER__MEASUREMENTS,
	K_DIR,
	K_DIR__MEASUREMENTS,
};

enum {
	I_STATS,
	I_LAST,
	I_COUNTER,
	I_DIR,
};

static const struct app_driver_item m_items[] = {
	[I_STATS] = APP_DRIVER_ITEM("stats", "", 0, APP_MODBUS_S32_HI, 1.f, APP_DRIVER_AGGREG_STATS,
				    1.f, K_STATS),
	[I_LAST] = APP_DRIVER_ITEM("last", "", 2, APP_MODBUS_U16, 1.f, APP_DRIVER_AGGREG_LAST, 1.f,
				   K_LAST),
	[I_COUNTER] = APP_DRIVER_ITEM("counter", "", 4, APP_MODBUS_U32_HI, 1.f,
				      APP_DRIVER_AGGREG_COUNTER, 1.f, K_COUNTER),
	[I_DIR] = APP_DRIVER_ITEM("dir", "", 6, APP_MODBUS_U16, 1.f, APP_DRIVER_AGGREG_CIRCULAR,
				  1.f, K_DIR),
};

static const struct app_driver_demand m_demand = {
	.items = m_items,
	.item_count = ARRAY_SIZE(m_items),
};

static uint8_t m_mem[1024] __aligned(8);
static struct app_driver_slot m_slot;

static void before(void *fixture)
{
	struct app_driver_slot_plan plan = {.win_cap = WIN_CAP, .samples_cap = SAMPLES_CAP};

	ARG_UNUSED(fixture);

	plan.bytes = app_driver_slot_bytes(&m_demand, 0, plan.win_cap, plan.samples_cap);
	zassert_true(plan.bytes <= sizeof(m_mem));

	memset(&m_slot, 0, sizeof(m_slot));
	app_driver_slot_setup(&m_slot, &m_demand, &plan, m_mem, INTERVAL);
}

static void put(size_t idx, float value)
{
	app_driver_items_put(&m_slot, idx, value);
}

/* Close the windows on the given number of timer ticks, at an RTC time with the uptime clock
 * moving in step with it
 */
static void aggreg_ticks(int64_t ts, uint32_t ticks)
{
	app_driver_items_aggreg(&m_slot, ts, (ts - T0 + 1000000) * 1000, ticks);
}

/* Close the windows on a single tick */
static void aggreg(int64_t ts)
{
	aggreg_ticks(ts, 1);
}

static uint32_t newest(void)
{
	return app_driver_win_end(&m_slot) - 1;
}

ZTEST(items, test_stats)
{
	put(I_STATS, 3.f);
	put(I_STATS, NAN); /* failed read: ignored */
	put(I_STATS, 1.f);
	put(I_STATS, 2.f);

	zassert_equal(app_driver_items_last(&m_slot, I_STATS), 2.f);

	aggreg(T0);

	const struct ctr_data_aggreg *w = app_driver_win_stats(&m_slot, I_STATS, newest());

	zassert_not_null(w);
	zassert_equal(w->min, 1.f);
	zassert_equal(w->max, 3.f);
	zassert_within(w->avg, 2.f, 1e-6f);
	zassert_equal(w->mdn, 2.f);
	zassert_within(app_driver_win_value(&m_slot, I_STATS, newest()), 2.f, 1e-6f);
}

ZTEST(items, test_stats_beyond_median_buffer)
{
	/* Running min / max / avg use every sample, the median the first SAMPLES_CAP */
	for (int i = 1; i <= 10; i++) {
		put(I_STATS, (float)i);
	}

	aggreg(T0);

	const struct ctr_data_aggreg *w = app_driver_win_stats(&m_slot, I_STATS, newest());

	zassert_equal(w->min, 1.f);
	zassert_equal(w->max, 10.f);
	zassert_within(w->avg, 5.5f, 1e-6f);
	zassert_equal(w->mdn, 3.f);
}

ZTEST(items, test_last_and_counter)
{
	put(I_LAST, 7.f);
	put(I_LAST, 8.f);
	put(I_COUNTER, 100.f);
	put(I_COUNTER, 104.5f);

	aggreg(T0);

	zassert_equal(app_driver_win_value(&m_slot, I_LAST, newest()), 8.f);
	zassert_equal(app_driver_win_value(&m_slot, I_COUNTER, newest()), 104.5f);
	zassert_is_null(app_driver_win_stats(&m_slot, I_COUNTER, newest()));
}

ZTEST(items, test_circular)
{
	put(I_DIR, 350.f);
	put(I_DIR, 10.f);
	aggreg(T0);

	float mean = app_driver_win_value(&m_slot, I_DIR, newest());

	zassert_true(mean < 0.01f || mean > 359.99f, "mean %f", (double)mean);

	put(I_DIR, 90.f);
	put(I_DIR, 180.f);
	aggreg(T0 + INTERVAL);
	zassert_within(app_driver_win_value(&m_slot, I_DIR, newest()), 135.f, 0.01f);

	put(I_DIR, 270.f);
	put(I_DIR, 300.f);
	aggreg(T0 + 2 * INTERVAL);
	zassert_within(app_driver_win_value(&m_slot, I_DIR, newest()), 285.f, 0.01f);

	/* Opposite directions: no mean direction */
	put(I_DIR, 0.f);
	put(I_DIR, 180.f);
	aggreg(T0 + 3 * INTERVAL);
	zassert_true(isnan(app_driver_win_value(&m_slot, I_DIR, newest())));
}

ZTEST(items, test_empty_window)
{
	put(I_STATS, 1.f);
	aggreg(T0);
	aggreg(T0 + INTERVAL);

	const struct ctr_data_aggreg *w = app_driver_win_stats(&m_slot, I_STATS, newest());

	zassert_true(isnan(w->min) && isnan(w->max) && isnan(w->avg) && isnan(w->mdn));
	zassert_true(isnan(app_driver_win_value(&m_slot, I_LAST, newest())));
	zassert_true(isnan(app_driver_win_value(&m_slot, I_DIR, newest())));

	/* The last sample survives the window */
	zassert_equal(app_driver_items_last(&m_slot, I_STATS), 1.f);
}

ZTEST(items, test_timeline)
{
	for (int i = 0; i < 3; i++) {
		put(I_STATS, (float)i);
		aggreg(T0 + i * INTERVAL);
	}

	zassert_equal(app_driver_win_count(&m_slot), 3);

	for (uint32_t seq = 0; seq < 3; seq++) {
		zassert_equal(app_driver_win_ts(&m_slot, seq), T0 + seq * INTERVAL);
	}
}

ZTEST(items, test_ring_drops_oldest)
{
	for (int i = 0; i < WIN_CAP + 2; i++) {
		put(I_STATS, (float)i);
		aggreg(T0 + i * INTERVAL);
	}

	zassert_equal(app_driver_win_count(&m_slot), WIN_CAP);
	zassert_equal(app_driver_win_first(&m_slot), 2);
	zassert_equal(m_slot.drops, 2);
	zassert_is_null(app_driver_win_stats(&m_slot, I_STATS, 1));
	zassert_equal(app_driver_win_stats(&m_slot, I_STATS, 2)->avg, 2.f);

	/* Timestamps of the remaining windows are unaffected */
	zassert_equal(app_driver_win_ts(&m_slot, 2), T0 + 2 * INTERVAL);
}

ZTEST(items, test_missed_ticks_are_filled)
{
	put(I_STATS, 1.f);
	aggreg(T0);

	/* The work queue was blocked: two ticks coalesced, this one 20 s late */
	put(I_STATS, 2.f);
	aggreg_ticks(T0 + 3 * INTERVAL + 20, 3);

	zassert_equal(app_driver_win_count(&m_slot), 4);
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 1)));
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 2)));
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 3), 2.f);
	zassert_equal(app_driver_win_ts(&m_slot, 3), T0 + 3 * INTERVAL);

	/* A late tick that did not miss a window adds no empty window */
	aggreg(T0 + 4 * INTERVAL + 100);
	zassert_equal(app_driver_win_end(&m_slot), 5);
}

ZTEST(items, test_late_tick_adds_no_window)
{
	put(I_STATS, 1.f);
	aggreg(T0);

	/* Closed more than half an interval late (behind a long LTE send), no tick missed */
	put(I_STATS, 2.f);
	aggreg(T0 + INTERVAL + INTERVAL * 6 / 10);
	zassert_equal(app_driver_win_count(&m_slot), 2);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 1), 2.f);

	/* The next tick is on time again */
	put(I_STATS, 3.f);
	aggreg(T0 + 2 * INTERVAL);
	zassert_equal(app_driver_win_count(&m_slot), 3);
	zassert_equal(m_slot.drops, 0);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 2), 3.f);

	for (uint32_t seq = 0; seq < 3; seq++) {
		zassert_equal(app_driver_win_ts(&m_slot, seq), T0 + seq * INTERVAL);
	}
}

ZTEST(items, test_coalesced_late_ticks)
{
	put(I_STATS, 1.f);
	aggreg(T0);

	/* Two ticks coalesced and this one runs more than half an interval late */
	put(I_STATS, 2.f);
	aggreg_ticks(T0 + 3 * INTERVAL + INTERVAL * 6 / 10, 3);

	zassert_equal(app_driver_win_count(&m_slot), 4);
	zassert_equal(m_slot.drops, 0);
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 1)));
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 2)));
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 3), 2.f);
	zassert_equal(app_driver_win_ts(&m_slot, 3), T0 + 3 * INTERVAL);

	put(I_STATS, 3.f);
	aggreg(T0 + 4 * INTERVAL);
	zassert_equal(app_driver_win_end(&m_slot), 5);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 4), 3.f);
	zassert_equal(app_driver_win_ts(&m_slot, 4), T0 + 4 * INTERVAL);
}

ZTEST(items, test_missed_ticks_on_empty_ring)
{
	put(I_STATS, 1.f);
	aggreg(T0);
	app_driver_win_pop_until(&m_slot, app_driver_win_end(&m_slot));

	/* Blocked for two ticks right after a report emptied the ring: there is nothing to keep
	 * apart, the timeline anchors at this window
	 */
	put(I_STATS, 2.f);
	aggreg_ticks(T0 + 3 * INTERVAL, 3);

	zassert_equal(app_driver_win_count(&m_slot), 1);
	zassert_equal(m_slot.drops, 0);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, newest()), 2.f);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 + 3 * INTERVAL);
}

ZTEST(items, test_no_tick_is_noop)
{
	/* The work ran again for an expiration the previous run already counted */
	aggreg_ticks(T0 - 10, 0);
	zassert_equal(app_driver_win_count(&m_slot), 0);

	put(I_STATS, 1.f);
	aggreg(T0);

	put(I_STATS, 2.f);
	aggreg_ticks(T0 + 10, 0);
	zassert_equal(app_driver_win_count(&m_slot), 1);

	/* The open window kept its samples */
	put(I_STATS, 4.f);
	aggreg(T0 + INTERVAL);
	zassert_equal(app_driver_win_count(&m_slot), 2);
	zassert_within(app_driver_win_value(&m_slot, I_STATS, 1), 3.f, 1e-6f);
	zassert_equal(app_driver_win_ts(&m_slot, 1), T0 + INTERVAL);
}

ZTEST(items, test_late_tick_is_not_a_clock_set)
{
	const int64_t late = INTERVAL * 9 / 10;

	put(I_STATS, 1.f);
	app_driver_items_aggreg(&m_slot, T0, 1000 * 1000, 1);

	/* Late by 0.9 interval; the RTC second rounds up */
	put(I_STATS, 2.f);
	app_driver_items_aggreg(&m_slot, T0 + INTERVAL + late + 1,
				(1000 + INTERVAL + late) * 1000 + 400, 1);
	zassert_equal(app_driver_win_count(&m_slot), 2);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 + INTERVAL);

	/* One tick missed, again late */
	put(I_STATS, 3.f);
	app_driver_items_aggreg(&m_slot, T0 + 3 * INTERVAL + late,
				(1000 + 3 * INTERVAL + late) * 1000, 2);
	zassert_equal(app_driver_win_count(&m_slot), 4);
	zassert_equal(m_slot.drops, 0);
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 2)));
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 3), 3.f);
	zassert_equal(app_driver_win_ts(&m_slot, 3), T0 + 3 * INTERVAL);
}

ZTEST(items, test_clock_set_with_missed_ticks)
{
	put(I_STATS, 1.f);
	app_driver_items_aggreg(&m_slot, T0, 1000 * 1000, 1);

	/* A long LTE attach blocked the work queue for three ticks and set the clock */
	put(I_STATS, 2.f);
	app_driver_items_aggreg(&m_slot, T0 + 86400 + 3 * INTERVAL, (1000 + 3 * INTERVAL) * 1000,
				3);

	/* The missed windows are kept apart, the timeline follows the new clock */
	zassert_equal(app_driver_win_count(&m_slot), 4);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 0), 1.f);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, 3), 2.f);
	zassert_equal(app_driver_win_ts(&m_slot, 0), T0 + 86400);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 + 86400 + 3 * INTERVAL);
}

ZTEST(items, test_reanchor_after_pop)
{
	put(I_STATS, 1.f);
	app_driver_items_aggreg(&m_slot, 1000, 1000 * 1000, 1);
	put(I_STATS, 2.f);
	app_driver_items_aggreg(&m_slot, 1000 + INTERVAL, (1000 + INTERVAL) * 1000, 1);

	/* Everything sent: the next window anchors the timeline again (the RTC was set
	 * in between, with an empty ring that needs no detection)
	 */
	app_driver_win_pop_until(&m_slot, app_driver_win_end(&m_slot));
	zassert_equal(app_driver_win_count(&m_slot), 0);

	put(I_STATS, 3.f);
	app_driver_items_aggreg(&m_slot, T0, (1000 + 2 * INTERVAL) * 1000, 1);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0);
}

ZTEST(items, test_clock_set)
{
	put(I_STATS, 1.f);
	app_driver_items_aggreg(&m_slot, T0, 1000 * 1000, 1);

	/* One interval later on the uptime clock, the RTC was set back */
	put(I_STATS, 2.f);
	app_driver_items_aggreg(&m_slot, T0 - 5000, (1000 + INTERVAL) * 1000, 1);
	zassert_equal(app_driver_win_count(&m_slot), 2);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 - 5000);

	/* ...and forward by a day: no empty windows for that day */
	put(I_STATS, 3.f);
	app_driver_items_aggreg(&m_slot, T0 + 86400, (1000 + 2 * INTERVAL) * 1000, 1);
	zassert_equal(app_driver_win_count(&m_slot), 3);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, newest()), 3.f);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 + 86400);
}

ZTEST(items, test_long_outage_restarts_ring)
{
	for (int i = 0; i < 3; i++) {
		put(I_STATS, (float)i);
		aggreg(T0 + i * INTERVAL);
	}

	/* The work queue was blocked for ten intervals: none of the buffered windows would
	 * survive the empty ones, the ring restarts with the current window
	 */
	put(I_STATS, 9.f);
	aggreg_ticks(T0 + 12 * INTERVAL, 10);

	zassert_equal(app_driver_win_count(&m_slot), 1);
	zassert_equal(m_slot.drops, 3);
	zassert_equal(app_driver_win_value(&m_slot, I_STATS, newest()), 9.f);
	zassert_equal(app_driver_win_ts(&m_slot, newest()), T0 + 12 * INTERVAL);
}

ZTEST(items, test_short_interval_jitter)
{
	struct app_driver_slot slot = {0};
	struct app_driver_slot_plan plan = {.win_cap = WIN_CAP, .samples_cap = SAMPLES_CAP};
	static uint8_t mem[1024] __aligned(8);

	plan.bytes = app_driver_slot_bytes(&m_demand, 0, plan.win_cap, plan.samples_cap);
	app_driver_slot_setup(&slot, &m_demand, &plan, mem, 1);

	/* 1 s windows closed 300 ms late and then on time: the RTC second ticks twice, no
	 * window was missed
	 */
	app_driver_items_aggreg(&slot, T0, 10000, 1);
	app_driver_items_aggreg(&slot, T0 + 2, 11300, 1);
	app_driver_items_aggreg(&slot, T0 + 3, 12000, 1);

	zassert_equal(app_driver_win_count(&slot), 3);
	zassert_equal(slot.drops, 0);
}

ZTEST(items, test_out_of_range)
{
	put(99, 1.f);
	zassert_true(isnan(app_driver_items_last(&m_slot, 99)));
	zassert_true(isnan(app_driver_win_value(&m_slot, I_STATS, 0)));
	zassert_is_null(app_driver_win_stats(&m_slot, I_STATS, 0));
}

ZTEST(items, test_aggreg_samples)
{
	float samples[] = {4.f, 1.f, 3.f, 2.f};
	struct ctr_data_aggreg out;

	app_driver_aggreg_samples(samples, ARRAY_SIZE(samples), &out);

	zassert_equal(out.min, 1.f);
	zassert_equal(out.max, 4.f);
	zassert_within(out.avg, 2.5f, 1e-6f);
	zassert_equal(out.mdn, 3.f);

	float with_nan[] = {1.f, NAN};

	app_driver_aggreg_samples(with_nan, ARRAY_SIZE(with_nan), &out);
	zassert_true(isnan(out.min) && isnan(out.avg));

	app_driver_aggreg_samples(samples, 0, &out);
	zassert_true(isnan(out.mdn));
}

ZTEST(items, test_value_to_int32)
{
	int32_t v;

	/* Rounded, not truncated: 0.251 * 1000 is 250.99998 in float */
	zassert_true(app_driver_value_to_int32(0.251f, 1000.f, &v));
	zassert_equal(v, 251);
	zassert_true(app_driver_value_to_int32(0.53f, 100.f, &v));
	zassert_equal(v, 53);
	zassert_true(app_driver_value_to_int32(12.7f, 1.f, &v));
	zassert_equal(v, 13);
	zassert_true(app_driver_value_to_int32(-12.7f, 1.f, &v));
	zassert_equal(v, -13);
	zassert_true(app_driver_value_to_int32(2.5f, 1.f, &v));
	zassert_equal(v, 3);

	/* Limits of the int32 range */
	zassert_true(app_driver_value_to_int32(-2147483648.f, 1.f, &v));
	zassert_equal(v, INT32_MIN);
	zassert_true(app_driver_value_to_int32(2147483520.f, 1.f, &v));
	zassert_equal(v, 2147483520);

	/* Not available or out of range: nil on the wire */
	zassert_false(app_driver_value_to_int32(NAN, 1000.f, &v));
	zassert_false(app_driver_value_to_int32(INFINITY, 1.f, &v));
	zassert_false(app_driver_value_to_int32(-INFINITY, 1.f, &v));
	zassert_false(app_driver_value_to_int32(2147483648.f, 1.f, &v));
	zassert_false(app_driver_value_to_int32(3e6f, 1000.f, &v));
}

ZTEST_SUITE(items, NULL, NULL, before, NULL, NULL);
