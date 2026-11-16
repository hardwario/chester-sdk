/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * ctr_cloud_init() and the API before it.
 */

#include "common.h"
#include "ctr_cloud_msg.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

/* ---- Before a successful ctr_cloud_init() (suite ctr_cloud_send_0_init) ---- */

static void assert_api_eperm(void)
{
	zassert_equal(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER), -EPERM);
	zassert_equal(ctr_cloud_send(PAYLOAD_A, sizeof(PAYLOAD_A)), -EPERM);
	zassert_equal(ctr_cloud_downlink(K_FOREVER), -EPERM);
	zassert_equal(ctr_cloud_poll_immediately(), -EPERM);
	zassert_equal(ctr_cloud_firmware_update("fw"), -EPERM);

	assert_no_transfer();
	zassert_equal(mock_transfer_downlink_count(), 0);
}

ZTEST(ctr_cloud_send_0_init, test_1_api_eperm_before_init)
{
	/* No ctr_cloud_init() at all yet: no options, transfer layer or work queue */
	assert_api_eperm();
}

/* Runs after test_2_cb_eperm_after_failed_init */
ZTEST(ctr_cloud_send_0_init, test_2_data_eperm_after_failed_init)
{
	assert_api_eperm();
}

/* ---- After ctr_cloud_init() ---- */

ZTEST_SUITE(ctr_cloud_send_4_init, NULL, common_setup_initialized, common_before, common_after,
	    NULL);

ZTEST(ctr_cloud_send_4_init, test_init_again_ealready)
{
	static struct ctr_cloud_options options = {
		.decoder_hash = 0xdeaddeaddeaddeadULL,
	};

	zassert_equal(ctr_cloud_init(&options), -EALREADY);

	/* Still the options of the first init: MSG_A carries their decoder hash */
	zassert_ok(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER));
	assert_uplink(0, MSG_A, sizeof(MSG_A));
}

static void match_thread_name(const struct k_thread *thread, void *user_data)
{
	const char *name = k_thread_name_get((k_tid_t)thread);
	bool *found = user_data;

	if (name && !strcmp(name, "ctr_cloud")) {
		*found = true;
	}
}

#if TEST_INIT_RETRY
/* A failed step is run again 10 s later, one that requires a session starts over from CREATE
 * SESSION at once
 */
ZTEST(ctr_cloud_send_4_init, test_init_steps_retried)
{
	/* clang-format off */
	static const struct {
		uint8_t type;
		int ret;
		/* Since the previous uplink */
		int64_t after_ms;
	} expected[] = {
		/* Without an id: the ack goes, then the decoder requires a session */
		{UL_CREATE_SESSION, 0, 0},
		{UL_UPLOAD_FIRMWARE, 0, 0},
		{UL_CREATE_SESSION, 0, 0},
		/* A failed step runs again, without a new session */
		{UL_UPLOAD_DECODER, -EIO, 0},
		{UL_UPLOAD_DECODER, 0, 10000},
		{UL_UPLOAD_ENCODER, 0, 0},
		/* Also the config, which used to be given up after a failed upload */
		{UL_UPLOAD_CONFIG, -EIO, 0},
		{UL_UPLOAD_CONFIG, -EIO, 10000},
		{UL_UPLOAD_CONFIG, 0, 10000},
	};
	/* clang-format on */

	zassert_true(mock_transfer_log_count() >= ARRAY_SIZE(expected), "%d uplinks",
		     mock_transfer_log_count());

	for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
		const struct mock_log_entry *e = mock_transfer_log(i);

		zassert_equal(e->mem[0], expected[i].type,
			      "uplink %zu of type 0x%02x, expected 0x%02x", i, e->mem[0],
			      expected[i].type);
		zassert_equal(e->ret, expected[i].ret, "uplink %zu failed with %d, expected %d", i,
			      e->ret, expected[i].ret);

		if (i > 0) {
			int64_t after_ms = e->at - mock_transfer_log(i - 1)->at;

			/* A sleep may end a tick late */
			zassert_within(after_ms, expected[i].after_ms + 10, 10,
				       "uplink %zu %lld ms after the previous one, expected %lld",
				       i, after_ms, expected[i].after_ms);
		}
	}
}
#endif

#if TEST_CONFIG_SHOW_FAIL
/* A config that cannot be packed is not retried: the init goes on without it, and data go
 * out
 */
ZTEST(ctr_cloud_send_4_init, test_init_config_show_fail)
{
	/* Initialized: the suite setup waited for it */
	zassert_equal(mock_transfer_log_find(UL_UPLOAD_CONFIG), -1);

	zassert_ok(ctr_cloud_send_data(PAYLOAD_A, sizeof(PAYLOAD_A), K_FOREVER));
	assert_uplink(0, MSG_A, sizeof(MSG_A));
}
#endif

ZTEST(ctr_cloud_send_4_init, test_work_queue_thread_name)
{
	bool found = false;

	k_thread_foreach(match_thread_name, &found);
	zassert_true(found, "no thread named ctr_cloud");
}
