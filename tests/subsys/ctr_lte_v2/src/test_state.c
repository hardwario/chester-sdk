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
#include <errno.h>
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

ZTEST(state, test_ceer_too_long_is_terminated)
{
	char ceer[sizeof(m_long)];

	/* None stored yet */
	zassert_equal(ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer)), -ENODATA);
	zassert_equal(ceer[0], '\0');

	ctr_lte_v2_state_set_ceer(m_long);

	memset(ceer, 'x', sizeof(ceer));
	zassert_ok(ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer)));
	/* The report buffer holds 31 characters and the NUL */
	zassert_equal(bounded_len(ceer), CTR_LTE_V2_STATE_CEER_SIZE - 1);
	zassert_mem_equal(ceer, m_long, CTR_LTE_V2_STATE_CEER_SIZE - 1);

	/* Cut to a smaller buffer of the caller */
	memset(ceer, 'x', sizeof(ceer));
	zassert_ok(ctr_lte_v2_state_get_ceer(ceer, 8));
	zassert_equal(bounded_len(ceer), 7);
	zassert_mem_equal(ceer, m_long, 7);
	zassert_equal(ceer[8], 'x', "written past the size");
}

ZTEST(state, test_cereg_is_reject)
{
	struct ctr_lte_v2_cereg_param param = {
		.valid = true,
		.stat = CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING,
		.cause_type = 0,
		.reject_cause = 15,
	};

	zassert_true(ctr_lte_v2_state_cereg_is_reject(&param));

	param.stat = CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED;
	zassert_true(ctr_lte_v2_state_cereg_is_reject(&param));

	/* An EPS-only attach is accepted with EMM cause 18 (CS domain not available) */
	param.reject_cause = 18;
	param.stat = CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING;
	zassert_false(ctr_lte_v2_state_cereg_is_reject(&param));
	param.stat = CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME;
	zassert_false(ctr_lte_v2_state_cereg_is_reject(&param));

	/* A manufacturer-specific cause is not an EMM cause */
	param.stat = CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING;
	param.cause_type = 1;
	zassert_false(ctr_lte_v2_state_cereg_is_reject(&param));

	/* No cause */
	param.cause_type = 0;
	param.reject_cause = 0;
	zassert_false(ctr_lte_v2_state_cereg_is_reject(&param));
}

ZTEST_SUITE(state, NULL, NULL, NULL, NULL, NULL);
