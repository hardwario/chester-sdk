/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the saturating reset-record ring, driven against the standalone
 * mock retained_mem device.
 */

#include "mock_retmem.h"

/* Compile the subsystem source in directly for white-box access to its static
 * reset-record ring helpers. */
#include "ctr_trace.c"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/ztest.h>

#include <stdint.h>

#define REASONS_OFF 0
#define CAP         4

/* Renamed from m_dev to avoid clashing with the static m_dev in the #included
 * ctr_trace.c. This points at the standalone scratch device. */
static const struct device *test_dev;

static void test_before(void *f)
{
	ARG_UNUSED(f);
	test_dev = mock_test_dev();
	mock_test_reset();
}

ZTEST_SUITE(ctr_trace_reason, NULL, NULL, test_before, NULL, NULL);

static void append_rec(uint16_t *head, uint16_t *count, uint16_t boot_num, uint32_t cause)
{
	struct ctr_trace_boot_rec rec = {0};

	rec.boot_num = boot_num;
	rec.reset_cause = cause;
	zassert_ok(reason_append(test_dev, REASONS_OFF, CAP, head, count, &rec));
}

ZTEST(ctr_trace_reason, test_below_capacity)
{
	uint16_t head = 0, count = 0;

	append_rec(&head, &count, 1, RESET_POR);
	append_rec(&head, &count, 2, RESET_SOFTWARE);

	zassert_equal(count, 2);

	struct ctr_trace_boot_rec out[CAP];
	uint16_t n = 0;

	zassert_ok(reason_get(test_dev, REASONS_OFF, CAP, head, count, out, CAP, &n));
	zassert_equal(n, 2);
	zassert_equal(out[0].boot_num, 1, "oldest first");
	zassert_equal(out[1].boot_num, 2);
	zassert_equal(out[0].reset_cause, RESET_POR);
	zassert_equal(out[1].reset_cause, RESET_SOFTWARE);
}

ZTEST(ctr_trace_reason, test_overflow_saturates)
{
	uint16_t head = 0, count = 0;

	/* Append CAP + 3 records; only the most recent CAP survive. */
	for (uint16_t i = 1; i <= CAP + 3; i++) {
		append_rec(&head, &count, i, RESET_WATCHDOG);
	}

	zassert_equal(count, CAP, "count must saturate at capacity");

	struct ctr_trace_boot_rec out[CAP];
	uint16_t n = 0;

	zassert_ok(reason_get(test_dev, REASONS_OFF, CAP, head, count, out, CAP, &n));
	zassert_equal(n, CAP);

	/* Retained = the last CAP boot numbers, oldest-of-retained first. */
	uint16_t first = (CAP + 3) - CAP + 1;

	for (uint16_t i = 0; i < CAP; i++) {
		zassert_equal(out[i].boot_num, first + i, "retained record %u wrong", i);
	}
}

ZTEST(ctr_trace_reason, test_get_clamps_to_max)
{
	uint16_t head = 0, count = 0;

	for (uint16_t i = 1; i <= CAP; i++) {
		append_rec(&head, &count, i, 0);
	}

	struct ctr_trace_boot_rec out[CAP];
	uint16_t n = 999;

	zassert_ok(reason_get(test_dev, REASONS_OFF, CAP, head, count, out, 2, &n));
	zassert_equal(n, 2, "get must clamp to max");
	zassert_equal(out[0].boot_num, 1);
	zassert_equal(out[1].boot_num, 2);
}
