/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Characterization of the buffer API (ctr_cloud_send_data(), ctr_cloud_send()) through
 * the real ctr_cloud.c: message bytes, spool, metrics, deadline and return codes. Uses
 * only the API that existed before ctr_cloud_send_data_cb(), so it runs unchanged
 * against the previous ctr_cloud.c as well.
 */

#include "common.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

ZTEST_SUITE(ctr_cloud_send_1_data, NULL, common_setup_initialized, common_before, common_after,
	    NULL);

ZTEST(ctr_cloud_send_1_data, test_einval)
{
	zassert_equal(ctr_cloud_send_data(NULL, 4, K_FOREVER), -EINVAL);
	zassert_equal(ctr_cloud_send_data(PAYLOAD_A, 0, K_FOREVER), -EINVAL);
	zassert_equal(ctr_cloud_send(NULL, 4), -EINVAL);
	zassert_equal(ctr_cloud_send(PAYLOAD_A, 0), -EINVAL);

	assert_no_transfer();
}

ZTEST(ctr_cloud_send_1_data, test_msg)
{
	uint32_t base = uplink_data_count();

	zassert_ok(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER));

	assert_one_data_uplink(MSG_A, sizeof(MSG_A), base);
	assert_spooled_once(MSG_A, sizeof(MSG_A), 1);
	zassert_equal(mock_spool_stored(), 0);

	/* ctr_cloud_send() is the same with K_FOREVER */
	zassert_ok(ctr_cloud_send(PAYLOAD_B, sizeof(PAYLOAD_B)));

	zassert_equal(mock_transfer_uplink_count(), 2);
	assert_uplink(1, MSG_B, sizeof(MSG_B));
	zassert_true(K_TIMEOUT_EQ(mock_transfer_uplink(1)->timeout, K_FOREVER));
	zassert_equal(uplink_data_count(), base + 2);
}

static int send_a(void)
{
	return ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER);
}

static int send_b(void)
{
	return ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_FOREVER);
}

static int send_b_deadline(void)
{
	return ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_MSEC(1000));
}

ZTEST(ctr_cloud_send_1_data, test_error_kept_then_drained)
{
	run_error_kept_then_drained(send_a, send_b);
}

ZTEST(ctr_cloud_send_1_data, test_spool_save_error_still_sends)
{
	if (!IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		ztest_test_skip();
	}

	uint32_t base = uplink_data_count();

	mock_spool_fail_next_save(-EIO);

	zassert_ok(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER));

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	zassert_equal(mock_spool_save_count(), 0);
	zassert_equal(mock_spool_delete_count(), 0);
	zassert_equal(uplink_data_count(), base + 1);
}

ZTEST(ctr_cloud_send_1_data, test_spool_delete_error_no_drain)
{
	if (!IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		ztest_test_skip();
	}

	uint32_t base = uplink_data_count();

	/* An older unsent message waits in the spool */
	zassert_ok(ctr_cloud_spool_save(MSG_B, sizeof(MSG_B), NULL));

	mock_spool_fail_next_delete(-EIO);

	/* Sent, so 0 even though the delete failed; the spool is not drained */
	zassert_ok(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER));

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	zassert_equal(mock_spool_load_count(), 0);
	zassert_equal(mock_spool_stored(), 2);
	zassert_equal(uplink_data_count(), base + 1);
}

ZTEST(ctr_cloud_send_1_data, test_enospc)
{
	static uint8_t big[TEST_BUF_SIZE - TEST_HDR_LEN + 1];
	uint32_t base = uplink_data_count();

	memset(big, 0x5a, sizeof(big));

	zassert_equal(ctr_cloud_send_data(big, sizeof(big), K_FOREVER), -ENOSPC);

	assert_no_transfer();
	zassert_equal(uplink_data_count(), base);

	/* The lock is usable after the error */
	assert_lock_free();

	/* Exactly full transfer buffer is fine (too big for the mock spool: sent without it) */
	zassert_ok(ctr_cloud_send_data(big, sizeof(big) - 1, K_FOREVER));

	zassert_equal(mock_transfer_uplink_count(), 1);
	zassert_equal(mock_transfer_uplink(0)->len, TEST_BUF_SIZE);
	zassert_mem_equal(mock_transfer_uplink(0)->mem, MSG_A, TEST_HDR_LEN);
	zassert_equal(mock_transfer_uplink(0)->mem[TEST_HDR_LEN], 0x5a);
}

ZTEST(ctr_cloud_send_1_data, test_lock_busy)
{
	mock_transfer_gate_next_uplink();
	helper_start(send_a_forever, NULL);
	zassert_ok(mock_transfer_wait_entered(K_SECONDS(5)));

	/* The raw k_mutex_lock() codes */
	zassert_equal(ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_NO_WAIT), -EBUSY);
	zassert_equal(ctr_cloud_send_data(PAYLOAD_B, sizeof(PAYLOAD_B), K_MSEC(10)), -EAGAIN);

	mock_transfer_release();
	zassert_ok(helper_join());

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	assert_spooled_once(MSG_A, sizeof(MSG_A), 1);
}

ZTEST(ctr_cloud_send_1_data, test_deadline_lock_wait)
{
	assert_deadline_after_lock_wait(send_b_deadline);
}
