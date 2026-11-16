/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <ctr_lte_v2_state.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <string.h>

/* Longer than every string kept by ctr_lte_v2_state */
static const char m_long[] = "0123456789012345678901234567890123456789"
			     "0123456789012345678901234567890123456789";

/* Length of the string, without reading past the longest one it can be */
static size_t bounded_len(const char *str)
{
	const char *end = memchr(str, '\0', sizeof(m_long));

	zassert_not_null(end, "string not terminated");

	return end - str;
}

ZTEST(state, test_iccid_too_long_is_terminated)
{
	char *iccid;

	ctr_lte_v2_state_set_iccid(m_long);

	zassert_ok(ctr_lte_v2_state_get_iccid(&iccid));
	/* The ICCID buffer holds 22 characters and the NUL */
	zassert_equal(bounded_len(iccid), 22);
	zassert_mem_equal(iccid, m_long, 22);
}

ZTEST(state, test_fw_version_too_long_is_terminated)
{
	char *version;

	ctr_lte_v2_state_set_modem_fw_version(m_long);

	zassert_ok(ctr_lte_v2_state_get_modem_fw_version(&version));
	/* The version buffer holds 63 characters and the NUL */
	zassert_equal(bounded_len(version), 63);
	zassert_mem_equal(version, m_long, 63);
}

ZTEST_SUITE(state, NULL, NULL, NULL, NULL, NULL);
