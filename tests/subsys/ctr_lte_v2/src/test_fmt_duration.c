/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdint.h>

static void expect(uint32_t sec, const char *exp)
{
	char buf[24];

	zassert_ok(ctr_util_fmt_duration(sec, buf, sizeof(buf)));
	zassert_str_equal(buf, exp, "%u s -> '%s'", sec, buf);
}

ZTEST(util_fmt_duration, test_units)
{
	expect(0, "0s");
	expect(45, "45s");
	expect(2210, "36m 50s");
	expect(3723, "1h 02m 03s");
	expect(93784, "1d 02h 03m 04s");
	expect(UINT32_MAX, "49710d 06h 28m 15s");
}

ZTEST(util_fmt_duration, test_too_small)
{
	char buf[4];

	zassert_equal(ctr_util_fmt_duration(2210, buf, sizeof(buf)), -ENOSPC);
}

ZTEST_SUITE(util_fmt_duration, NULL, NULL, NULL, NULL, NULL);
