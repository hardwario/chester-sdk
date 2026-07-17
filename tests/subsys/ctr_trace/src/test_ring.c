/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the circular log-region primitives (append / read) and the
 * fault-snapshot copy, driven against the standalone mock retained_mem device.
 */

#include "mock_retmem.h"

/* Compile the subsystem source in directly for white-box access to its static
 * ring/snapshot helpers. */
#include "ctr_trace.c"

static int log_read(const struct device *dev, off_t data_off, size_t size, uint32_t head,
		    uint32_t count, uint8_t *out, size_t max, size_t *copied)
{
	int ret;
	size_t n = MIN((size_t)count, max);

	if (n == 0 || size == 0) {
		*copied = 0;
		return 0;
	}

	uint32_t oldest = (uint32_t)(((size_t)head + size - count) % size);
	size_t first = MIN(n, size - oldest);

	ret = retained_mem_read(dev, data_off + (off_t)oldest, out, first);
	if (ret) {
		return ret;
	}

	if (n > first) {
		ret = retained_mem_read(dev, data_off, out + first, n - first);
		if (ret) {
			return ret;
		}
	}

	*copied = n;

	return 0;
}

#include <zephyr/ztest.h>

#include <stdint.h>
#include <string.h>

#define RING_OFF  0
#define RING_SIZE 256
#define SNAP_OFF  1024

/* Renamed from m_dev to avoid clashing with the static m_dev in the #included
 * ctr_trace.c. This points at the standalone scratch device. */
static const struct device *test_dev;

static void test_before(void *f)
{
	ARG_UNUSED(f);
	test_dev = mock_test_dev();
	mock_test_reset();
}

ZTEST_SUITE(ctr_trace_ring, NULL, NULL, test_before, NULL, NULL);

/* Append a run of `n` bytes starting at value `start` (wrapping mod 256). */
static void append_seq(uint32_t *head, uint32_t *count, uint8_t start, size_t n)
{
	uint8_t tmp[512];

	for (size_t i = 0; i < n; i++) {
		tmp[i] = (uint8_t)(start + i);
	}
	zassert_ok(log_append(test_dev, RING_OFF, RING_SIZE, head, count, tmp, n));
}

ZTEST(ctr_trace_ring, test_empty)
{
	uint8_t out[RING_SIZE];
	size_t copied = 12345;

	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, 0, 0, out, sizeof(out), &copied));
	zassert_equal(copied, 0, "empty ring must read 0 bytes");
}

ZTEST(ctr_trace_ring, test_below_capacity)
{
	uint32_t head = 0, count = 0;
	uint8_t out[RING_SIZE];
	size_t copied = 0;

	append_seq(&head, &count, 0, 100);
	zassert_equal(count, 100);
	zassert_equal(head, 100);

	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, head, count, out, sizeof(out), &copied));
	zassert_equal(copied, 100);
	for (size_t i = 0; i < 100; i++) {
		zassert_equal(out[i], (uint8_t)i, "byte %d mismatch (oldest first)", (int)i);
	}
}

ZTEST(ctr_trace_ring, test_overflow_saturates)
{
	uint32_t head = 0, count = 0;
	uint8_t out[RING_SIZE];
	size_t copied = 0;

	/* Write 700 bytes into a 256-byte ring; only the last 256 survive. */
	append_seq(&head, &count, 0, 300);
	append_seq(&head, &count, (uint8_t)300, 400);

	zassert_equal(count, RING_SIZE, "count must saturate at ring size");

	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, head, count, out, sizeof(out), &copied));
	zassert_equal(copied, RING_SIZE);

	/* Total written = 700; last 256 correspond to values 444..699 (mod 256). */
	for (size_t i = 0; i < RING_SIZE; i++) {
		uint8_t expect = (uint8_t)((700 - RING_SIZE) + i);

		zassert_equal(out[i], expect, "retained byte %d mismatch", (int)i);
	}
}

ZTEST(ctr_trace_ring, test_single_write_larger_than_ring)
{
	uint32_t head = 0, count = 0;
	uint8_t out[RING_SIZE];
	size_t copied = 0;

	/* One append bigger than the ring keeps only its last RING_SIZE bytes. */
	append_seq(&head, &count, 0, 400);

	zassert_equal(count, RING_SIZE);
	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, head, count, out, sizeof(out), &copied));
	zassert_equal(copied, RING_SIZE);
	for (size_t i = 0; i < RING_SIZE; i++) {
		zassert_equal(out[i], (uint8_t)((400 - RING_SIZE) + i), "byte %d mismatch", (int)i);
	}
}

ZTEST(ctr_trace_ring, test_read_clamps_to_max)
{
	uint32_t head = 0, count = 0;
	uint8_t out[RING_SIZE];
	size_t copied = 0;

	append_seq(&head, &count, 10, 50);

	/* Poison the destination beyond `max` to confirm it is not written. */
	memset(out, 0xAA, sizeof(out));

	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, head, count, out, 20, &copied));
	zassert_equal(copied, 20, "read must clamp to max");
	for (size_t i = 0; i < 20; i++) {
		zassert_equal(out[i], (uint8_t)(10 + i));
	}
	zassert_equal(out[20], 0xAA, "read must not write past max");

	/* max == 0 copies nothing. */
	copied = 999;
	zassert_ok(log_read(test_dev, RING_OFF, RING_SIZE, head, count, out, 0, &copied));
	zassert_equal(copied, 0);
}

ZTEST(ctr_trace_ring, test_snapshot)
{
	uint32_t head = 0, count = 0;

	/* Fill the ring past capacity so the logical content wraps. */
	append_seq(&head, &count, 0, 400);
	zassert_equal(count, RING_SIZE);

	uint32_t snap_len = 0;

	zassert_ok(snapshot(test_dev, RING_OFF, SNAP_OFF, RING_SIZE, head, count, &snap_len));
	zassert_equal(snap_len, RING_SIZE, "snapshot must copy the whole valid content");

	/* The snapshot is linear and oldest-first: read it straight back. */
	uint8_t snap[RING_SIZE];

	zassert_ok(log_read(test_dev, SNAP_OFF, RING_SIZE, RING_SIZE, RING_SIZE, snap, sizeof(snap),
			    &(size_t){0}));

	/* Also read raw so we compare linear bytes exactly. */
	uint8_t raw[RING_SIZE];

	memcpy(raw, mock_test_backing + SNAP_OFF, RING_SIZE);
	for (size_t i = 0; i < RING_SIZE; i++) {
		zassert_equal(raw[i], (uint8_t)((400 - RING_SIZE) + i), "snapshot byte %d mismatch",
			      (int)i);
	}
}

ZTEST(ctr_trace_ring, test_snapshot_empty)
{
	uint32_t snap_len = 999;

	zassert_ok(snapshot(test_dev, RING_OFF, SNAP_OFF, RING_SIZE, 0, 0, &snap_len));
	zassert_equal(snap_len, 0, "snapshot of empty ring copies nothing");
}
