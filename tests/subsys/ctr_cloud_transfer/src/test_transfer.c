/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Retries of ctr_cloud_transfer_uplink() and ctr_cloud_transfer_downlink() when an exchange fails
 * or gets a stale response, over a scripted ctr_lte_v2_send_recv()
 */

#include "ctr_cloud_packet.h"
#include "ctr_cloud_transfer.h"
#include "mock.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#define SERIAL_NUMBER 2159018247

#define UPLINK (CTR_CLOUD_PACKET_FLAG_FIRST | CTR_CLOUD_PACKET_FLAG_LAST)
#define POLL   CTR_CLOUD_PACKET_FLAG_POLL
#define ACK    CTR_CLOUD_PACKET_FLAG_ACK
#define LAST   CTR_CLOUD_PACKET_FLAG_LAST
#define DATA   (CTR_CLOUD_PACKET_FLAG_FIRST | CTR_CLOUD_PACKET_FLAG_LAST)

/* clang-format off */
/* The exchange of the packet fails after ms */
#define FAIL(seq, fl, err, ms) \
	{.sequence = (seq), .flags = (fl), .ret = (err), .duration_ms = (ms)}
/* The packet gets the response at once */
#define REPLY(seq, fl, rseq, rfl, data) \
	{.sequence = (seq), .flags = (fl), .resp_sequence = (rseq), .resp_flags = (rfl), \
	 .resp_data = (data)}
/* The final ACK of a downlink, which expects no response, fails after ms */
#define ACK_FAIL(seq, err, ms) \
	{.sequence = (seq), .flags = ACK, .no_recv = true, .ret = (err), .duration_ms = (ms)}
#define ACK_OK(seq) \
	{.sequence = (seq), .flags = ACK, .no_recv = true}
/* clang-format on */

static uint8_t m_token[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
			      0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};

CTR_BUF_DEFINE_STATIC(m_buf, 64);

static int uplink(k_timeout_t timeout, int64_t *elapsed)
{
	ctr_buf_reset(&m_buf);
	zassert_ok(ctr_buf_append_mem(&m_buf, "Hello", 5));

	int64_t start = k_uptime_get();
	int ret = ctr_cloud_transfer_uplink(&m_buf, NULL, timeout);
	*elapsed = k_uptime_get() - start;

	return ret;
}

static int downlink(k_timeout_t timeout, int64_t *elapsed)
{
	ctr_buf_reset(&m_buf);

	int64_t start = k_uptime_get();
	int ret = ctr_cloud_transfer_downlink(&m_buf, NULL, timeout);
	*elapsed = k_uptime_get() - start;

	return ret;
}

/* Assert that the call started at_ms after the first one, with the timeout; each sleep before it
 * may end a tick or so late */
static void assert_call(size_t index, int64_t at_ms, int64_t timeout_ms)
{
	const struct mock_lte_call *call = mock_lte_call(index);
	int64_t at = call->start - mock_lte_call(0)->start;
	int64_t delta = 20 * (index + 1);

	zassert_within(at, at_ms, delta, "call %zu at %lld ms, expected %lld ms", index, at, at_ms);

	if (timeout_ms == MOCK_LTE_FOREVER) {
		zassert_equal(call->timeout_ms, MOCK_LTE_FOREVER,
			      "call %zu with timeout %lld ms, expected K_FOREVER", index,
			      call->timeout_ms);
	} else {
		zassert_within(call->timeout_ms, timeout_ms, delta,
			       "call %zu with timeout %lld ms, expected %lld ms", index,
			       call->timeout_ms, timeout_ms);
	}
}

/* A lost response: the same packet again 1 s later, with the time left */
ZTEST(transfer, test_lost_reply)
{
	static const struct mock_lte_exchange script[] = {
		FAIL(0, UPLINK, -ETIMEDOUT, 5000),
		REPLY(0, UPLINK, 1, 0, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "uplink failed: %d", ret);

	assert_call(0, 0, 60000);
	assert_call(1, 6000, 54000);
	mock_lte_assert_done();
}

/* No connection granted: the same packet again 30 s later */
ZTEST(transfer, test_no_conn)
{
	static const struct mock_lte_exchange script[] = {
		FAIL(0, UPLINK, -ENOTCONN, 3000),
		REPLY(0, UPLINK, 1, 0, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_SECONDS(120), &elapsed);
	zassert_ok(ret, "uplink failed: %d", ret);

	assert_call(1, 33000, 87000);
	mock_lte_assert_done();
}

/* Each attempt gets the time left, and the backoff ends at the deadline */
ZTEST(transfer, test_deadline)
{
	static const struct mock_lte_exchange script[] = {
		FAIL(0, UPLINK, -ETIMEDOUT, 5000),
		/* The backoff of 30 s is cut to the 29 s left */
		FAIL(0, UPLINK, -ENOTCONN, 10000),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_SECONDS(45), &elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_within(elapsed, 45000, 20, "returned %lld ms after the call", elapsed);

	assert_call(1, 6000, 39000);
	mock_lte_assert_done();
}

/* The final ACK of a downlink, which gets no response, is sent again after no connection */
ZTEST(transfer, test_ack_no_conn)
{
	static const struct mock_lte_exchange script[] = {
		REPLY(0, POLL, 1, DATA, "abc"),
		ACK_FAIL(2, -ENOTCONN, 2000),
		ACK_OK(2),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = downlink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "downlink failed: %d", ret);
	zassert_equal(ctr_buf_get_used(&m_buf), 3, "downlink of %zu bytes",
		      ctr_buf_get_used(&m_buf));
	zassert_mem_equal(ctr_buf_get_mem(&m_buf), "abc", 3);

	assert_call(2, 32000, 28000);
	mock_lte_assert_done();
}

/* ... but not after a lost response, as it expects none */
ZTEST(transfer, test_ack_lost_reply)
{
	static const struct mock_lte_exchange script[] = {
		REPLY(0, POLL, 1, DATA, "abc"),
		ACK_FAIL(2, -ETIMEDOUT, 2000),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = downlink(K_SECONDS(60), &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);

	mock_lte_assert_done();
}

/* A stale response to an uplink: the same packet with the same sequence, at most 3 times, then a
 * restart 1 s later */
ZTEST(transfer, test_stale_uplink)
{
	static const struct mock_lte_exchange script[] = {
		REPLY(0, UPLINK, 1, 0, NULL),
		/* The response to the previous packet, 4 times */
		REPLY(2, UPLINK, 1, 0, NULL),
		REPLY(2, UPLINK, 1, 0, NULL),
		REPLY(2, UPLINK, 1, 0, NULL),
		REPLY(2, UPLINK, 1, 0, NULL),
		/* Restarted */
		REPLY(0, UPLINK, 1, 0, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "uplink failed: %d", ret);

	ret = uplink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "uplink failed: %d", ret);

	assert_call(4, 0, 60000);
	assert_call(5, 1000, 59000);
	mock_lte_assert_done();
}

/* The same for a downlink */
ZTEST(transfer, test_stale_downlink)
{
	static const struct mock_lte_exchange script[] = {
		REPLY(0, POLL, 1, LAST, NULL),
		/* The response to the previous packet, 4 times */
		REPLY(2, POLL, 1, LAST, NULL),
		REPLY(2, POLL, 1, LAST, NULL),
		REPLY(2, POLL, 1, LAST, NULL),
		REPLY(2, POLL, 1, LAST, NULL),
		/* Restarted */
		REPLY(0, POLL, 1, LAST, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = downlink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "downlink failed: %d", ret);

	ret = downlink(K_SECONDS(60), &elapsed);
	zassert_ok(ret, "downlink failed: %d", ret);

	assert_call(4, 0, 60000);
	assert_call(5, 1000, 59000);
	mock_lte_assert_done();
}

/* A restart waits 1 s, and not past the deadline */
ZTEST(transfer, test_restart_deadline)
{
	static const struct mock_lte_exchange script[] = {
		/* An unexpected sequence each time */
		REPLY(0, UPLINK, 77, 0, NULL),
		REPLY(0, UPLINK, 77, 0, NULL),
		REPLY(0, UPLINK, 77, 0, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_MSEC(2500), &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_within(elapsed, 2500, 20, "returned %lld ms after the call", elapsed);

	assert_call(1, 1000, 1500);
	assert_call(2, 2000, 500);
	mock_lte_assert_done();
}

/* The same for a downlink */
ZTEST(transfer, test_restart_deadline_downlink)
{
	static const struct mock_lte_exchange script[] = {
		REPLY(0, POLL, 77, LAST, NULL),
		REPLY(0, POLL, 77, LAST, NULL),
		REPLY(0, POLL, 77, LAST, NULL),
	};

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = downlink(K_MSEC(2500), &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_within(elapsed, 2500, 20, "returned %lld ms after the call", elapsed);

	assert_call(1, 1000, 1500);
	assert_call(2, 2000, 500);
	mock_lte_assert_done();
}

/* K_FOREVER retries on */
ZTEST(transfer, test_forever)
{
	/* clang-format off */
	static const struct mock_lte_exchange script[] = {
		FAIL(0, UPLINK, -ENOTCONN, 1000),
		FAIL(0, UPLINK, -ENOTCONN, 1000),
		FAIL(0, UPLINK, -ENOTCONN, 1000),
		FAIL(0, UPLINK, -ETIMEDOUT, 5000),
		FAIL(0, UPLINK, -ETIMEDOUT, 5000),
		REPLY(0, UPLINK, 1, 0, NULL),
	};
	/* clang-format on */

	mock_lte_start(SERIAL_NUMBER, m_token, script, ARRAY_SIZE(script));

	int64_t elapsed;
	int ret = uplink(K_FOREVER, &elapsed);
	zassert_ok(ret, "uplink failed: %d", ret);

	assert_call(0, 0, MOCK_LTE_FOREVER);
	assert_call(1, 31000, MOCK_LTE_FOREVER);
	assert_call(2, 62000, MOCK_LTE_FOREVER);
	assert_call(3, 93000, MOCK_LTE_FOREVER);
	assert_call(4, 99000, MOCK_LTE_FOREVER);
	assert_call(5, 105000, MOCK_LTE_FOREVER);
	mock_lte_assert_done();
}

static void before(void *fixture)
{
	/* From sequence 0 */
	zassert_ok(ctr_cloud_transfer_init(SERIAL_NUMBER, m_token, NULL));
}

ZTEST_SUITE(transfer, NULL, NULL, before, NULL, NULL);
