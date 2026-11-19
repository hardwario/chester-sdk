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

/* After its own CFUN=4 the modem reports not registered: the values of the last registration
 * stay, until the radio is on again */
ZTEST(state, test_cereg_kept_while_radio_off)
{
	struct ctr_lte_v2_cereg_param reg = {
		.valid = true,
		.stat = CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING,
		.cid = 0xAE520,
		.act = CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT,
		.active_time = -1,
		.periodic_tau_ext = -1,
	};
	struct ctr_lte_v2_cereg_param off = {
		.valid = true,
		.stat = CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED,
	};
	struct ctr_lte_v2_cereg_param out;

	strcpy(reg.tac, "B4DC");
	ctr_lte_v2_state_set_cereg_param(&reg);

	ctr_lte_v2_state_set_radio_off(true);
	ctr_lte_v2_state_set_cereg_param(&off);

	zassert_ok(ctr_lte_v2_state_get_cereg_param(&out));
	zassert_equal(out.stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_str_equal(out.tac, "B4DC");
	zassert_equal(out.cid, 0xAE520);
	zassert_equal(out.active_time, -1);

	/* A registration is taken anyway */
	reg.cid = 0xAE521;
	ctr_lte_v2_state_set_cereg_param(&reg);
	zassert_ok(ctr_lte_v2_state_get_cereg_param(&out));
	zassert_equal(out.cid, 0xAE521);

	/* Radio on: a loss of the network is stored */
	ctr_lte_v2_state_set_radio_off(false);
	off.stat = CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING;
	ctr_lte_v2_state_set_cereg_param(&off);

	zassert_ok(ctr_lte_v2_state_get_cereg_param(&out));
	zassert_equal(out.stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING);
}

ZTEST_SUITE(state, NULL, NULL, NULL, NULL, NULL);

static void add_cereg(int stat, const char *tac, int cid, int cause_type, int cause)
{
	struct ctr_lte_v2_cereg_param param = {
		.valid = true,
		.stat = stat,
		.cid = cid,
		.act = CTR_LTE_V2_CEREG_PARAM_ACT_LTE,
		.cause_type = cause_type,
		.reject_cause = cause,
	};

	strcpy(param.tac, tac);
	ctr_lte_v2_state_add_cereg_event(&param);
}

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0

/* The parser configuration keeps 3 changes */
BUILD_ASSERT(CONFIG_CTR_LTE_V2_CEREG_HISTORY == 3);

ZTEST(state_cereg_history, test_history)
{
	struct ctr_lte_v2_cereg_event e[5];
	size_t count;

	zassert_equal(ctr_lte_v2_state_get_last_reject(&e[0]), -ENODATA);

	add_cereg(2, "8DCC", 0xAE5CA, 0, 0);
	/* A repeat is not stored */
	add_cereg(2, "8DCC", 0xAE5CA, 0, 0);
	add_cereg(3, "8DCC", 0xAE5CA, 0, 15);

	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, ARRAY_SIZE(e), &count));
	zassert_equal(count, 2);
	zassert_equal(e[0].reject_cause, 15, "newest first");
	zassert_equal(e[0].tac, 0x8DCC);
	zassert_equal(e[0].cid, 0xAE5CA);
	zassert_equal(e[1].stat, 2);

	/* A manufacturer-specific cause is not an EMM cause */
	add_cereg(3, "8DCC", 0xAE5CA, 1, 42);

	/* From here on the last connection evaluation is on the cell 074FEB50: its PLMN for the
	 * changes on that cell */
	struct ctr_lte_v2_conn_param conn = {.valid = true, .plmn = 23002, .cid = 0x74FEB50};

	ctr_lte_v2_state_set_conn_param(&conn);

	/* An accepted registration keeps its cause, but is not a reject */
	add_cereg(5, "05F2", 0x74FEB50, 0, 18);
	add_cereg(1, "05F2", 0x74FEB50, 0, 0);

	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, ARRAY_SIZE(e), &count));
	zassert_equal(count, 3, "the ring keeps the newest 3");
	zassert_equal(e[0].stat, 1);
	zassert_equal(e[0].plmn, 23002);
	zassert_equal(e[1].stat, 5);
	zassert_equal(e[1].reject_cause, 18);
	zassert_equal(e[1].plmn, 23002);
	zassert_equal(e[2].stat, 3);
	zassert_equal(e[2].reject_cause, 0);
	zassert_equal(e[2].plmn, 0, "before the evaluation");

	/* Fewer than kept */
	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, 1, &count));
	zassert_equal(count, 1);
	zassert_equal(e[0].stat, 1);

	/* The reject survives the ring rolling over */
	zassert_ok(ctr_lte_v2_state_get_last_reject(&e[0]));
	zassert_equal(e[0].reject_cause, 15);
	zassert_equal(e[0].tac, 0x8DCC);
	zassert_equal(e[0].stat, 3);
	zassert_equal(e[0].plmn, 0);

	/* A reject on another cell (e.g. by another network): not the PLMN of the evaluation */
	add_cereg(2, "2D02", 0x74FEB56, 0, 11);
	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, 1, &count));
	zassert_equal(e[0].reject_cause, 11);
	zassert_equal(e[0].plmn, 0);
	zassert_ok(ctr_lte_v2_state_get_last_reject(&e[0]));
	zassert_equal(e[0].reject_cause, 11);
	zassert_equal(e[0].plmn, 0);

	/* Back on the cell of the evaluation */
	add_cereg(1, "05F2", 0x74FEB50, 0, 0);
	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, 1, &count));
	zassert_equal(e[0].plmn, 23002);

	/* An evaluation that is not valid: unknown */
	conn.valid = false;
	ctr_lte_v2_state_set_conn_param(&conn);
	add_cereg(2, "05F2", 0x74FEB50, 0, 0);
	zassert_ok(ctr_lte_v2_state_get_cereg_history(e, 1, &count));
	zassert_equal(e[0].plmn, 0);
}

#else

ZTEST(state_cereg_history, test_no_history)
{
	struct ctr_lte_v2_cereg_event e;
	size_t count = 1;

	add_cereg(3, "8DCC", 0xAE5CA, 0, 15);

	zassert_ok(ctr_lte_v2_state_get_cereg_history(&e, 1, &count));
	zassert_equal(count, 0);

	/* The last reject is kept anyway */
	zassert_ok(ctr_lte_v2_state_get_last_reject(&e));
	zassert_equal(e.reject_cause, 15);
}

#endif /* CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0 */

ZTEST(state_cereg_history, test_null_args)
{
	struct ctr_lte_v2_cereg_event e;
	size_t count;

	zassert_equal(ctr_lte_v2_state_get_cereg_history(NULL, 1, &count), -EINVAL);
	zassert_equal(ctr_lte_v2_state_get_cereg_history(&e, 1, NULL), -EINVAL);
	zassert_equal(ctr_lte_v2_state_get_last_reject(NULL), -EINVAL);
}

ZTEST_SUITE(state_cereg_history, NULL, NULL, NULL, NULL, NULL);
