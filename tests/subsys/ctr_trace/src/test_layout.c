/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Pure-logic tests for the retained-region partitioning (layout_compute) and the
 * fault decision (reset_is_fault). No device required.
 *
 * The subsystem source is compiled in directly for white-box access to its
 * static helpers.
 */

#include "ctr_trace.c"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/ztest.h>

#include <errno.h>
#include <stdint.h>

ZTEST_SUITE(ctr_trace_layout, NULL, NULL, NULL, NULL, NULL);

ZTEST(ctr_trace_layout, test_layout_valid)
{
	struct ctr_trace_layout l;
	size_t region = 8192;
	uint16_t reasons = 8;

	zassert_ok(layout_compute(region, reasons, &l));

	size_t hdr = sizeof(struct ctr_trace_header);
	size_t reason_bytes = (size_t)reasons * sizeof(struct ctr_trace_boot_rec);

	/* Reset records sit right after the header. */
	zassert_equal((size_t)l.reasons_off, hdr);
	zassert_equal(l.reason_cap, reasons);

	/* Primary follows the reset records; secondary follows the primary. */
	zassert_equal((size_t)l.primary_off, hdr + reason_bytes);
	zassert_equal((size_t)l.secondary_off, hdr + reason_bytes + l.log_size);

	/* Both log buffers are equal and account for the whole remainder. */
	zassert_true(l.log_size >= CTR_TRACE_MIN_LOG_SIZE);
	zassert_true((size_t)l.secondary_off + l.log_size <= region,
		     "layout must not exceed the region");
	zassert_equal(l.log_size, (region - hdr - reason_bytes) / 2);
}

ZTEST(ctr_trace_layout, test_layout_too_small)
{
	struct ctr_trace_layout l;

	/* A region that cannot hold header + records + two minimal log buffers. */
	size_t hdr = sizeof(struct ctr_trace_header);
	size_t tiny = hdr + 8 * sizeof(struct ctr_trace_boot_rec) + CTR_TRACE_MIN_LOG_SIZE;

	zassert_equal(layout_compute(tiny, 8, &l), -ENOMEM,
		      "must reject a region too small for two log buffers");
}

ZTEST(ctr_trace_layout, test_layout_boundary)
{
	struct ctr_trace_layout l;
	size_t hdr = sizeof(struct ctr_trace_header);
	uint16_t reasons = 4;
	size_t reason_bytes = (size_t)reasons * sizeof(struct ctr_trace_boot_rec);

	/* Exactly large enough: header + records + 2 * minimal log buffer. */
	size_t exact = hdr + reason_bytes + 2 * CTR_TRACE_MIN_LOG_SIZE;

	zassert_ok(layout_compute(exact, reasons, &l));
	zassert_equal(l.log_size, CTR_TRACE_MIN_LOG_SIZE);

	/* One byte short must fail. */
	zassert_equal(layout_compute(exact - 1, reasons, &l), -ENOMEM);
}

ZTEST(ctr_trace_layout, test_fault_decision)
{
	/* Marker alone is a fault. */
	zassert_true(reset_is_fault(0, true));

	/* Watchdog and lockup are faults. */
	zassert_true(reset_is_fault(RESET_WATCHDOG, false));
	zassert_true(reset_is_fault(RESET_CPU_LOCKUP, false));
	zassert_true(reset_is_fault(RESET_WATCHDOG | RESET_PIN, false));

	/* A plain software or power-on reset is NOT a fault (clean reboot looks
	 * identical to a crash-triggered software reset). */
	zassert_false(reset_is_fault(RESET_SOFTWARE, false));
	zassert_false(reset_is_fault(RESET_POR, false));
	zassert_false(reset_is_fault(RESET_PIN, false));
	zassert_false(reset_is_fault(0, false));

	/* But a software reset with the marker set is a fault. */
	zassert_true(reset_is_fault(RESET_SOFTWARE, true));
}
