/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#if defined(CONFIG_CTR_LTE_V2_CAUSE_STR)

ZTEST(str_cause, test_emm_cause)
{
	zassert_str_equal(ctr_lte_v2_str_emm_cause(15), "No suitable cells in tracking area");
	zassert_not_null(ctr_lte_v2_str_emm_cause_hint(15));
	zassert_str_equal(ctr_lte_v2_str_emm_cause(111), "Protocol error, unspecified");
}

ZTEST(str_cause, test_esm_cause)
{
	zassert_str_equal(ctr_lte_v2_str_esm_cause(27), "Missing or unknown APN");
	zassert_not_null(ctr_lte_v2_str_esm_cause_hint(27));
}

ZTEST(str_cause, test_unknown_cause)
{
	zassert_str_equal(ctr_lte_v2_str_emm_cause(0), "unknown");
	zassert_str_equal(ctr_lte_v2_str_esm_cause(255), "unknown");
	zassert_is_null(ctr_lte_v2_str_emm_cause_hint(0));
	/* A known cause without a hint */
	zassert_is_null(ctr_lte_v2_str_emm_cause_hint(16));
	zassert_is_null(ctr_lte_v2_str_esm_cause_hint(36));
}

#else

/* Without the texts every cause is a bare number */
ZTEST(str_cause, test_no_texts)
{
	zassert_str_equal(ctr_lte_v2_str_emm_cause(15), "unknown");
	zassert_is_null(ctr_lte_v2_str_emm_cause_hint(15));
	zassert_str_equal(ctr_lte_v2_str_esm_cause(27), "unknown");
	zassert_is_null(ctr_lte_v2_str_esm_cause_hint(27));
}

#endif /* defined(CONFIG_CTR_LTE_V2_CAUSE_STR) */

ZTEST_SUITE(str_cause, NULL, NULL, NULL, NULL, NULL);
