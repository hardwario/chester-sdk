/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * ctr_cloud_send_data_cb() through the real ctr_cloud.c: the callback encodes into the
 * transfer buffer and the mock transport sees the exact message bytes. Also the -EDEADLK
 * guard of every cloud API that rewrites the transfer buffer.
 */

#include "common.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Third-party includes */
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

/* ---- Callbacks ---- */

static int m_calls;

static K_SEM_DEFINE(m_cb_entered, 0, 1);
static K_SEM_DEFINE(m_cb_release, 0, 1);

/* {1: 42} as PAYLOAD_A */
static int encode_a(zcbor_state_t *zs, void *user_data)
{
	m_calls++;

	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 1);
	zcbor_uint32_put(zs, 42);
	zcbor_map_end_encode(zs, 1);

	return 0;
}

/* [7, 8] as PAYLOAD_B */
static int encode_b(zcbor_state_t *zs, void *user_data)
{
	m_calls++;

	return ctr_cloud_cbor_put_raw(zs, PAYLOAD_B, sizeof(PAYLOAD_B));
}

static int encode_a_after_ms(zcbor_state_t *zs, void *user_data)
{
	k_msleep(*(int *)user_data);

	return encode_a(zs, NULL);
}

/* ---- Up to a successful ctr_cloud_init() (suites and tests run in name order) ---- */

static void *init_setup(void)
{
	/* Before the first ctr_cloud_init(), which confirms a firmware update */
	mock_fw_setup();

	return NULL;
}

ZTEST_SUITE(ctr_cloud_send_0_init, NULL, init_setup, common_before, common_after, NULL);

ZTEST(ctr_cloud_send_0_init, test_0_cb_null_einval)
{
	/* -EINVAL is checked before -EPERM */
	zassert_equal(ctr_cloud_send_data_cb(NULL, NULL, K_FOREVER), -EINVAL);

	assert_no_transfer();
}

ZTEST(ctr_cloud_send_0_init, test_1_cb_eperm_before_init)
{
	/* No ctr_cloud_init() at all yet (no options) */
	m_calls = 0;

	zassert_equal(ctr_cloud_send_data_cb(encode_a, NULL, K_FOREVER), -EPERM);

	zassert_equal(m_calls, 0);
	assert_no_transfer();
}

ZTEST(ctr_cloud_send_0_init, test_2_cb_eperm_after_failed_init)
{
	static struct ctr_cloud_options options = {
		.decoder_hash = 0x0102030405060708ULL,
	};

	/* Fails after it has stored the options */
	mock_transfer_set_init_ret(-EIO);
	zassert_equal(ctr_cloud_init(&options), -EIO);
	mock_transfer_set_init_ret(0);

	m_calls = 0;

	zassert_equal(ctr_cloud_send_data_cb(encode_a, NULL, K_FOREVER), -EPERM);

	zassert_equal(m_calls, 0);
	assert_no_transfer();
}

static int send_a_forever_cb(void *arg)
{
	return ctr_cloud_send_data_cb(encode_a, NULL, K_FOREVER);
}

ZTEST(ctr_cloud_send_0_init, test_3_cb_waits_for_session)
{
	/* The init work holds the cloud lock while it creates the session */
	mock_transfer_gate_next_uplink();
	zassert_ok(common_init_start());

	/* Started right away: no -EPERM before the init work has run */
	int ret = ctr_cloud_send_data_cb(encode_a, NULL, K_NO_WAIT);

	zassert_true(ret == -ETIMEDOUT || ret == -EAGAIN, "ret %d", ret);

	zassert_ok(mock_transfer_wait_entered(K_SECONDS(5)));

	/* Started is not initialized while the session is being created */
	bool initialized = true;

	zassert_equal(ctr_cloud_wait_initialized(K_NO_WAIT), -ETIMEDOUT);
	zassert_ok(ctr_cloud_is_initialized(&initialized));
	zassert_false(initialized);

	m_calls = 0;

	/* Started, so no -EPERM: the send waits for the lock instead */
	helper_start(send_a_forever_cb, NULL);
	k_msleep(50);
	zassert_equal(m_calls, 0);

	mock_transfer_release();
	zassert_ok(helper_join());
	/* Posted after the init work releases the lock, so maybe only after the send */
	zassert_ok(ctr_cloud_wait_initialized(K_SECONDS(5)));

	/* Encoded and sent after the session (and the configuration upload) */
	int count = mock_transfer_uplink_count();

	zassert_equal(m_calls, 1);
	zassert_true(count >= 1);
	assert_uplink(count - 1, MSG_A, sizeof(MSG_A));
}

/* ---- After ctr_cloud_init() ---- */

static void before(void *fixture)
{
	common_before(fixture);
	m_calls = 0;
}

static void after(void *fixture)
{
	/* A failed test may have left the helper inside encode_a_holding() */
	k_sem_give(&m_cb_release);
	common_after(fixture);
}

ZTEST_SUITE(ctr_cloud_send_2_cb, NULL, common_setup_initialized, before, after, NULL);

struct msg_probe {
	int calls;
	void *user_data;
	size_t len_before;
	size_t free_before;
	size_t len_after;
};

static int encode_a_probe(zcbor_state_t *zs, void *user_data)
{
	struct msg_probe *p = user_data;

	p->calls++;
	p->user_data = user_data;
	p->len_before = ctr_cloud_cbor_len(zs);
	p->free_before = ctr_cloud_cbor_free(zs);

	encode_a(zs, NULL);

	p->len_after = ctr_cloud_cbor_len(zs);

	return 0;
}

ZTEST(ctr_cloud_send_2_cb, test_msg)
{
	struct msg_probe probe = {0};
	uint32_t base = uplink_data_count();

	zassert_ok(ctr_cloud_send_data_cb(encode_a_probe, &probe, K_FOREVER));

	zassert_equal(probe.calls, 1);
	zassert_equal_ptr(probe.user_data, &probe);
	zassert_equal(probe.len_before, 0);
	zassert_equal(probe.free_before, TEST_BUF_SIZE - TEST_HDR_LEN);
	zassert_equal(probe.len_after, sizeof(PAYLOAD_A));

	/* The same message as ctr_cloud_send_data(PAYLOAD_A) */
	assert_one_data_uplink(MSG_A, sizeof(MSG_A), base);
	assert_spooled_once(MSG_A, sizeof(MSG_A), 1);
	zassert_equal(mock_spool_stored(), 0);
}

static int encode_cancel(zcbor_state_t *zs, void *user_data)
{
	m_calls++;

	/* A partial message */
	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 1);

	return -ECANCELED;
}

static int encode_overflow(zcbor_state_t *zs, void *user_data)
{
	m_calls++;

	/* 5 bytes each, far more than the transfer buffer; errors deliberately ignored */
	zcbor_list_start_encode(zs, 1);
	for (int i = 0; i < 4000; i++) {
		zcbor_uint32_put(zs, 0x10000);
	}
	zcbor_list_end_encode(zs, 1);

	return 0;
}

static int encode_nothing(zcbor_state_t *zs, void *user_data)
{
	m_calls++;

	return 0;
}

static int encode_positive(zcbor_state_t *zs, void *user_data)
{
	encode_a(zs, NULL);

	return 1;
}

ZTEST(ctr_cloud_send_2_cb, test_encode_error)
{
	static const struct {
		ctr_cloud_data_encode_cb cb;
		int ret;
	} cases[] = {
		{encode_cancel, -ECANCELED},
		{encode_overflow, -ENOSPC},
		{encode_nothing, -EINVAL},
		{encode_positive, -EINVAL},
	};
	uint32_t base = uplink_data_count();

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		m_calls = 0;

		zassert_equal(ctr_cloud_send_data_cb(cases[i].cb, NULL, K_FOREVER), cases[i].ret,
			      "case %zu", i);

		/* Nothing sent, nothing spooled */
		zassert_equal(m_calls, 1, "case %zu", i);
		assert_no_transfer();
		zassert_equal(uplink_data_count(), base, "case %zu", i);
	}

	/* The lock and the transfer buffer are usable after the errors */
	assert_lock_free();

	zassert_ok(ctr_cloud_send_data_cb(encode_a, NULL, K_FOREVER));

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
}

static int send_a(void)
{
	return ctr_cloud_send_data_cb(encode_a, NULL, K_FOREVER);
}

static int send_b(void)
{
	return ctr_cloud_send_data_cb(encode_b, NULL, K_FOREVER);
}

static int send_b_deadline(void)
{
	return ctr_cloud_send_data_cb(encode_b, NULL, K_MSEC(1000));
}

ZTEST(ctr_cloud_send_2_cb, test_error_kept_then_drained)
{
	run_error_kept_then_drained(send_a, send_b);
}

/* ---- Lock and deadline ---- */

/* Holds the cloud lock inside the callback until released */
static int encode_a_holding(zcbor_state_t *zs, void *user_data)
{
	k_sem_give(&m_cb_entered);
	k_sem_take(&m_cb_release, K_FOREVER);

	return encode_a(zs, NULL);
}

static int send_holding_forever(void *arg)
{
	return ctr_cloud_send_data_cb(encode_a_holding, NULL, K_FOREVER);
}

ZTEST(ctr_cloud_send_2_cb, test_lock_wait_eagain)
{
	k_sem_reset(&m_cb_entered);
	k_sem_reset(&m_cb_release);

	helper_start(send_holding_forever, NULL);
	zassert_ok(k_sem_take(&m_cb_entered, K_SECONDS(5)));

	/* Another thread inside a callback: wait, never -EDEADLK; any lock failure is -EAGAIN */
	zassert_equal(ctr_cloud_send_data_cb(encode_b, NULL, K_NO_WAIT), -EAGAIN);
	zassert_equal(ctr_cloud_send_data_cb(encode_b, NULL, K_MSEC(10)), -EAGAIN);
	zassert_equal(m_calls, 0);

	/* The other API keeps the raw k_mutex_lock() codes */
	zassert_equal(ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_NO_WAIT), -EBUSY);
	zassert_equal(ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_MSEC(10)), -EAGAIN);
	zassert_equal(ctr_cloud_downlink(K_NO_WAIT), -EBUSY);

	k_sem_give(&m_cb_release);
	zassert_ok(helper_join());

	zassert_equal(m_calls, 1);
	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	zassert_equal(mock_transfer_downlink_count(), 0);
	assert_spooled_once(MSG_A, sizeof(MSG_A), 1);
}

ZTEST(ctr_cloud_send_2_cb, test_deadline_left_after_encode)
{
	int sleep_ms = 300;

	zassert_ok(ctr_cloud_send_data_cb(encode_a_after_ms, &sleep_ms, K_MSEC(1000)));

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));

	/* One deadline: the encoding time is taken from the transfer */
	assert_timeout_left(mock_transfer_uplink(0), 600, 700);
}

ZTEST(ctr_cloud_send_2_cb, test_deadline_left_after_lock_wait)
{
	assert_deadline_after_lock_wait(send_b_deadline);
}

ZTEST(ctr_cloud_send_2_cb, test_deadline_expired_after_encode)
{
	int sleep_ms = 200;
	uint32_t base = uplink_data_count();

	zassert_equal(ctr_cloud_send_data_cb(encode_a_after_ms, &sleep_ms, K_MSEC(100)),
		      -ETIMEDOUT);

	/* Encoded, not sent */
	zassert_equal(m_calls, 1);
	zassert_equal(mock_transfer_uplink_count(), 0);
	zassert_equal(mock_transfer_downlink_count(), 0);
	zassert_equal(uplink_data_count(), base);
	zassert_equal(mock_spool_delete_count(), 0);

	if (IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		/* Saved for a later send, without a transfer */
		size_t saved_len;
		const uint8_t *saved = mock_spool_last_saved(&saved_len);

		zassert_equal(mock_spool_save_count(), 1);
		zassert_equal(saved_len, sizeof(MSG_A));
		zassert_mem_equal(saved, MSG_A, sizeof(MSG_A));
		zassert_equal(mock_spool_stored(), 1);
	} else {
		zassert_equal(mock_spool_save_count(), 0);
		zassert_equal(mock_spool_stored(), 0);
	}

	assert_lock_free();

	/* A later successful send drains it: newest first, then the spooled one */
	zassert_ok(ctr_cloud_send_data_cb(encode_b, NULL, K_FOREVER));
	assert_uplink(0, MSG_B, sizeof(MSG_B));
	if (IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		zassert_equal(mock_transfer_uplink_count(), 2);
		assert_uplink(1, MSG_A, sizeof(MSG_A));
		zassert_equal(uplink_data_count(), base + 2);
	} else {
		zassert_equal(mock_transfer_uplink_count(), 1);
		zassert_equal(uplink_data_count(), base + 1);
	}
	zassert_equal(mock_spool_stored(), 0);
}

ZTEST(ctr_cloud_send_2_cb, test_deadline_expired_after_encode_save_fails)
{
	int sleep_ms = 200;
	uint32_t base = uplink_data_count();

	/* Only the deadline path saves, and its error must not replace -ETIMEDOUT */
	mock_spool_fail_next_save(-EIO);

	zassert_equal(ctr_cloud_send_data_cb(encode_a_after_ms, &sleep_ms, K_MSEC(100)),
		      -ETIMEDOUT);

	zassert_equal(m_calls, 1);
	zassert_equal(mock_transfer_uplink_count(), 0);
	zassert_equal(mock_transfer_downlink_count(), 0);
	zassert_equal(uplink_data_count(), base);
	zassert_equal(mock_spool_stored(), 0);

	assert_lock_free();
}

ZTEST(ctr_cloud_send_2_cb, test_deadline_no_wait)
{
	/* K_NO_WAIT leaves no time for the transfer: never encodes, never sends */
	zassert_equal(ctr_cloud_send_data_cb(encode_a, NULL, K_NO_WAIT), -ETIMEDOUT);

	zassert_equal(m_calls, 0);
	assert_no_transfer();

	assert_lock_free();
}

/* ---- Nested calls from the callback (-EDEADLK) ----
 *
 * The outer callback writes the start of PAYLOAD_A, makes the nested call and finishes
 * PAYLOAD_A. A nested call that got through would rewrite the transfer buffer (the
 * nested payloads differ from PAYLOAD_A in the first bytes) or call the transport.
 */

struct nested {
	int (*call)(void);
	int ret;
};

static int encode_a_nested(zcbor_state_t *zs, void *user_data)
{
	struct nested *n = user_data;

	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 1);

	n->ret = n->call();

	zcbor_uint32_put(zs, 42);
	zcbor_map_end_encode(zs, 1);

	return 0;
}

static void assert_nested_edeadlk(int (*call)(void))
{
	struct nested n = {.call = call, .ret = INT32_MIN};

	zassert_ok(ctr_cloud_send_data_cb(encode_a_nested, &n, K_FOREVER));

	zassert_equal(n.ret, -EDEADLK);

	/* Only the outer message, unchanged; no transport call by the nested one */
	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	zassert_equal(mock_transfer_downlink_count(), 0);
	assert_spooled_once(MSG_A, sizeof(MSG_A), 1);

	/* The nested call left the lock balanced */
	assert_lock_free();
}

static int nested_send_data(void)
{
	return ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_FOREVER);
}

static int nested_send_data_no_wait(void)
{
	return ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_NO_WAIT);
}

static int nested_send(void)
{
	return ctr_cloud_send(PAYLOAD_B, sizeof(PAYLOAD_B));
}

static int nested_send_data_cb(void)
{
	return ctr_cloud_send_data_cb(encode_b, NULL, K_FOREVER);
}

static int nested_send_data_cb_no_wait(void)
{
	return ctr_cloud_send_data_cb(encode_b, NULL, K_NO_WAIT);
}

static int nested_downlink(void)
{
	return ctr_cloud_downlink(K_FOREVER);
}

static int nested_firmware_update(void)
{
	return ctr_cloud_firmware_update("fw-1");
}

ZTEST(ctr_cloud_send_2_cb, test_nested_send_data)
{
	assert_nested_edeadlk(nested_send_data);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_send_data_no_wait)
{
	assert_nested_edeadlk(nested_send_data_no_wait);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_send)
{
	assert_nested_edeadlk(nested_send);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_send_data_cb)
{
	assert_nested_edeadlk(nested_send_data_cb);

	/* encode_b never ran */
	zassert_equal(m_calls, 0);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_send_data_cb_no_wait)
{
	/* -EDEADLK takes precedence over the expired deadline */
	assert_nested_edeadlk(nested_send_data_cb_no_wait);
	zassert_equal(m_calls, 0);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_downlink)
{
	assert_nested_edeadlk(nested_downlink);
}

ZTEST(ctr_cloud_send_2_cb, test_nested_firmware_update)
{
	assert_nested_edeadlk(nested_firmware_update);
}
