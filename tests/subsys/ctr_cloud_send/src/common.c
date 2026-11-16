/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "common.h"
#include "mock.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <string.h>

#define HELPER_STACK_SIZE 4096
#define HELPER_PRIORITY   K_PRIO_PREEMPT(1)

const uint8_t PAYLOAD_A[5] = {0xbf, 0x01, 0x18, 0x2a, 0xff};
const uint8_t MSG_A[TEST_HDR_LEN + 5] = {0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					 0x07, 0x08, 0xbf, 0x01, 0x18, 0x2a, 0xff};
const uint8_t PAYLOAD_B[3] = {0x82, 0x07, 0x08};
const uint8_t MSG_B[TEST_HDR_LEN + 3] = {0x06, 0x01, 0x02, 0x03, 0x04, 0x05,
					 0x06, 0x07, 0x08, 0x82, 0x07, 0x08};

#if TEST_INIT_RETRY
/* Uploaded by the init work */
static const uint8_t m_decoder[] = {'d', 'e', 'c'};
static const uint8_t m_encoder[] = {'e', 'n', 'c'};
#endif

/* Different hashes, so that a message with the wrong one is told apart */
static struct ctr_cloud_options m_options = {
	.decoder_hash = 0x0102030405060708ULL,
	.encoder_hash = 0x1112131415161718ULL,
#if TEST_INIT_RETRY
	.decoder_buf = m_decoder,
	.decoder_len = sizeof(m_decoder),
	.encoder_buf = m_encoder,
	.encoder_len = sizeof(m_encoder),
#endif
};

static K_THREAD_STACK_DEFINE(m_helper_stack, HELPER_STACK_SIZE);
static struct k_thread m_helper_thread;
static int (*m_helper_fn)(void *arg);
static void *m_helper_arg;
static int m_helper_ret;
static bool m_helper_running;

int common_init_start(void)
{
	static bool started;

	if (started) {
		return 0;
	}

	/* The shell must be up before the init work uploads the configuration */
	mock_global_setup_suite();

	started = true;

	return ctr_cloud_init(&m_options);
}

void *common_setup_initialized(void)
{
	zassert_ok(common_init_start());
	zassert_ok(ctr_cloud_wait_initialized(TEST_INIT_TIMEOUT));

	return NULL;
}

void common_before(void *fixture)
{
	ARG_UNUSED(fixture);

	mock_transfer_reset();
	mock_spool_reset();
}

uint32_t uplink_data_count(void)
{
	struct ctr_cloud_metrics metrics;

	zassert_ok(ctr_cloud_get_metrics(&metrics));

	return metrics.uplink_data_count;
}

void assert_uplink(int index, const uint8_t *msg, size_t len)
{
	const struct mock_uplink *u = mock_transfer_uplink(index);

	zassert_not_null(u, "no uplink %d", index);
	zassert_equal(u->len, len, "uplink %d length %zu", index, u->len);
	zassert_mem_equal(u->mem, msg, len, "uplink %d bytes", index);
}

void assert_no_transfer(void)
{
	zassert_equal(mock_transfer_uplink_count(), 0);
	zassert_equal(mock_transfer_downlink_count(), 0);
	zassert_equal(mock_spool_save_count(), 0);
	zassert_equal(mock_spool_stored(), 0);
}

void assert_timeout_left(const struct mock_uplink *u, uint32_t min_ms, uint32_t max_ms)
{
	zassert_false(K_TIMEOUT_EQ(u->timeout, K_FOREVER));
	zassert_true(u->timeout.ticks > (k_ticks_t)k_ms_to_ticks_floor64(min_ms),
		     "timeout %lld ticks too short", (long long)u->timeout.ticks);
	zassert_true(u->timeout.ticks <= (k_ticks_t)k_ms_to_ticks_ceil64(max_ms),
		     "timeout %lld ticks too long", (long long)u->timeout.ticks);
}

static void helper_entry(void *p1, void *p2, void *p3)
{
	m_helper_ret = m_helper_fn(m_helper_arg);
}

void helper_start(int (*fn)(void *arg), void *arg)
{
	m_helper_fn = fn;
	m_helper_arg = arg;
	m_helper_ret = INT32_MIN;
	m_helper_running = true;

	k_thread_create(&m_helper_thread, m_helper_stack, K_THREAD_STACK_SIZEOF(m_helper_stack),
			helper_entry, NULL, NULL, NULL, HELPER_PRIORITY, 0, K_NO_WAIT);
}

int helper_join(void)
{
	zassert_ok(k_thread_join(&m_helper_thread, K_SECONDS(10)), "helper thread stuck");
	m_helper_running = false;

	return m_helper_ret;
}

void common_after(void *fixture)
{
	ARG_UNUSED(fixture);

	if (!m_helper_running) {
		return;
	}

	/* A failed test left the helper behind: let it finish, so that later tests run */
	mock_transfer_release();
	if (k_thread_join(&m_helper_thread, K_SECONDS(2))) {
		k_thread_abort(&m_helper_thread);
	}
	m_helper_running = false;
}

void assert_spooled_once(const uint8_t *msg, size_t len, int deleted)
{
	if (!IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		zassert_equal(mock_spool_save_count(), 0);
		return;
	}

	size_t saved_len;
	const uint8_t *saved = mock_spool_last_saved(&saved_len);

	zassert_equal(mock_spool_save_count(), 1);
	zassert_equal(saved_len, len);
	zassert_mem_equal(saved, msg, len);
	zassert_equal(mock_spool_delete_count(), deleted);
}

void assert_one_data_uplink(const uint8_t *msg, size_t len, uint32_t base)
{
	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, msg, len);
	zassert_true(K_TIMEOUT_EQ(mock_transfer_uplink(0)->timeout, K_FOREVER));
	zassert_equal(mock_transfer_downlink_count(), 0);
	zassert_equal(uplink_data_count(), base + 1);
}

int send_a_forever(void *arg)
{
	return ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER);
}

void assert_deadline_after_lock_wait(int (*send_b)(void))
{
	mock_transfer_gate_next_uplink();
	helper_start(send_a_forever, NULL);
	zassert_ok(mock_transfer_wait_entered(K_SECONDS(5)));

	/* The other sender holds the lock for 300 ms of the 1000 ms deadline */
	mock_transfer_release_after(K_MSEC(300));

	zassert_ok(send_b());
	zassert_ok(helper_join());

	zassert_equal(mock_transfer_uplink_count(), 2);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	assert_uplink(1, MSG_B, sizeof(MSG_B));

	/* The transfer gets what is left of the deadline */
	assert_timeout_left(mock_transfer_uplink(1), 600, 700);
}

void run_error_kept_then_drained(send_fn send_a, send_fn send_b)
{
	uint32_t base = uplink_data_count();

	mock_transfer_fail_next_uplink(-EIO);

	zassert_equal(send_a(), -EIO);

	zassert_equal(mock_transfer_uplink_count(), 1);
	assert_uplink(0, MSG_A, sizeof(MSG_A));
	zassert_equal(uplink_data_count(), base);
	zassert_equal(mock_spool_stored(), IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL) ? 1 : 0);
	assert_spooled_once(MSG_A, sizeof(MSG_A), 0);

	/* The lock is usable after the error */
	assert_lock_free();

	zassert_ok(send_b());

	/* Newest first, then the spooled one */
	assert_uplink(1, MSG_B, sizeof(MSG_B));

	if (IS_ENABLED(CONFIG_CTR_CLOUD_SPOOL)) {
		zassert_equal(mock_transfer_uplink_count(), 3);
		assert_uplink(2, MSG_A, sizeof(MSG_A));
		zassert_equal(mock_spool_load_count(), 1);
		zassert_equal(uplink_data_count(), base + 2);
	} else {
		zassert_equal(mock_transfer_uplink_count(), 2);
		zassert_equal(uplink_data_count(), base + 1);
	}
	zassert_equal(mock_spool_stored(), 0);
}

static int downlink_no_wait(void *arg)
{
	return ctr_cloud_downlink(K_NO_WAIT);
}

void assert_lock_free(void)
{
	helper_start(downlink_no_wait, NULL);
	zassert_ok(helper_join(), "cloud lock not free for another thread");
}
