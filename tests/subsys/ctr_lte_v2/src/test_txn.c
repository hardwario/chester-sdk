/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * The send_recv transaction when the caller gives up. Each scenario is a configuration of its
 * own (testcase.yaml) and runs from the attach.
 */

#include "stack.h"

#include "ctr_lte_v2_state.h"

#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CANARY 0xa5

static __maybe_unused int send_recv(struct ctr_lte_v2_send_recv_param *param, int64_t *elapsed)
{
	int64_t start = k_uptime_get();
	int ret = ctr_lte_v2_send_recv(param);
	*elapsed = k_uptime_get() - start;

	return ret;
}

static K_THREAD_STACK_DEFINE(m_caller_stack, 2048);
static struct k_thread m_caller_thread;
static struct ctr_lte_v2_send_recv_param *m_caller_param;
static int64_t m_caller_elapsed;
static int m_caller_ret;

static void caller_entry(void *p1, void *p2, void *p3)
{
	m_caller_ret = send_recv(m_caller_param, &m_caller_elapsed);
}

/* Call send_recv from a thread of the priority instead of the (cooperative) test thread */
static __maybe_unused void caller_start(struct ctr_lte_v2_send_recv_param *param, int prio)
{
	m_caller_param = param;

	k_thread_create(&m_caller_thread, m_caller_stack, K_THREAD_STACK_SIZEOF(m_caller_stack),
			caller_entry, NULL, NULL, NULL, prio, 0, K_NO_WAIT);
}

static __maybe_unused int caller_join(int64_t *elapsed)
{
	zassert_ok(k_thread_join(&m_caller_thread, K_SECONDS(60)));

	*elapsed = m_caller_elapsed;

	return m_caller_ret;
}

static __maybe_unused void assert_canary(const char *buf, size_t size, const size_t *len)
{
	for (size_t i = 0; i < size; i++) {
		zassert_equal((uint8_t)buf[i], CANARY, "recv_buf[%zu] written after the return", i);
	}

	zassert_equal(*len, CANARY, "recv_len written after the return");
}

static __maybe_unused void sleep_until(int64_t uptime)
{
	int64_t now = k_uptime_get();

	if (uptime > now) {
		k_sleep(K_MSEC(uptime - now));
	}
}

/* Give a transaction up while the FSM is in a state without IO: the call returns at the deadline,
 * and the state is left alone */
static __maybe_unused void give_up(const char *state)
{
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.timeout = K_SECONDS(2),
		.rai = true,
	};

	int64_t elapsed;
	int ret = send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2000 && elapsed <= 2100, "returned %lld ms after the call",
		     elapsed);

	zassert_equal(strcmp(ctr_lte_v2_get_state(), state), 0, "expected state %s but got: %s",
		      state, ctr_lte_v2_get_state());
}

static __maybe_unused void go_to_sleep(void)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_SLEEP(100),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	stack_wait_for_state("sleep", K_SECONDS(2));
	/* Past the pause after AT#XSLEEP */
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}

#if defined(CONFIG_TEST_FEATURE_TXN_GIVE_UP_IN_RECV)
/* The caller gives up during AT#XRECV, the response comes past its deadline */
ZTEST(txn, test_give_up_in_recv)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* RRC still connected after the attach: on to receive at once */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		RX_DATA(3000, "#XRECV: 8", "Hi there"),
		/* No error state, the FSM goes on */
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_SLEEP(5000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(2),
		.rai = true,
	};

	/* The call waits for the end of AT#XRECV, the response is dropped. The caller runs at the
	 * priority of the FSM, which then ends its work on the response before the caller wakes
	 * up: the result is still the caller's, as it gave the transaction up first. */
	caller_start(&param, K_LOWEST_APPLICATION_THREAD_PRIO);

	int64_t elapsed;
	int ret = caller_join(&elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2900 && elapsed <= 3500, "returned %lld ms after the call",
		     elapsed);

	memset(recv_buf, CANARY, sizeof(recv_buf));
	recv_len = CANARY;

	k_sleep(K_SECONDS(8));

	assert_canary(recv_buf, sizeof(recv_buf), &recv_len);

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_GIVE_UP_IN_ATTACH)
/* The caller gives up in the attach and in the retry delay, their timers run on */
ZTEST(txn, test_give_up_in_attach)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Attempt 1: no network for 5 minutes, no retry delay */
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		/* Attempt 2: the same */
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		/* Attempt 3: no network for 50 minutes, retry delay of an hour */
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		/* Attempt 4: attached */
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	ctr_lte_v2_enable();

	stack_wait_for_state("attach", K_SECONDS(10));
	int64_t attach_start = k_uptime_get();

	give_up("attach");

	struct ctr_lte_v2_metrics metrics;
	zassert_ok(ctr_lte_v2_get_metrics(&metrics));
	zassert_equal(metrics.attach_fail_count, 0, "attach failed: %u", metrics.attach_fail_count);

	int remaining_sec;
	zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec));
	zassert_true(remaining_sec >= 300 - 3, "attach timer cut to %d s", remaining_sec);

	/* The attach times out at its time */
	sleep_until(attach_start + 299 * MSEC_PER_SEC);
	ASSERT_FSM_STATE("attach");
	stack_wait_for_state("retry_delay", K_SECONDS(2));

	/* On to the retry delay after the third attempt */
	for (int i = 0; i < 4000; i++) {
		zassert_ok(ctr_lte_v2_get_metrics(&metrics));
		if (metrics.attach_fail_count == 3) {
			break;
		}
		k_sleep(K_SECONDS(1));
	}
	zassert_equal(metrics.attach_fail_count, 3, "attach failed: %u", metrics.attach_fail_count);
	ASSERT_FSM_STATE("retry_delay");

	/* Until its timer runs */
	for (int i = 0; i < 100; i++) {
		zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec));
		if (remaining_sec > 0) {
			break;
		}
		k_sleep(K_MSEC(100));
	}
	int64_t retry_delay_start = k_uptime_get();

	int retry_delay_sec;
	zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, &retry_delay_sec, NULL));
	zassert_equal(retry_delay_sec, 3600, "retry delay: %d s", retry_delay_sec);

	give_up("retry_delay");

	zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec));
	zassert_true(remaining_sec >= 3600 - 4, "retry delay cut to %d s", remaining_sec);

	/* The retry delay runs to its end */
	sleep_until(retry_delay_start + 3598 * MSEC_PER_SEC);
	ASSERT_FSM_STATE("retry_delay");

	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(30)));
	k_sleep(K_MSEC(500));

	zassert_ok(ctr_lte_v2_get_metrics(&metrics));
	zassert_equal(metrics.attach_fail_count, 3, "attach failed: %u", metrics.attach_fail_count);

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_GIVE_UP_IN_ERROR)
/* The caller gives up in the error state, its timer runs on */
ZTEST(txn, test_give_up_in_error)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		/* The modem refuses the send */
		{"AT#XSOCKETOPT=1,51", "ERROR"},
		/* Error state - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Ready state, nothing to send any more */
		ITEMS_SLEEP(6000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	/* The send fails, the deadline comes in the error state */
	int64_t start = k_uptime_get();
	give_up("error");

	/* The error state ends at its time */
	sleep_until(start + 4800);
	ASSERT_FSM_STATE("error");
	stack_wait_for_state("ready", K_MSEC(500));

	stack_wait_for_state("sleep", K_SECONDS(2));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_GIVE_UP_IN_SEND)
/* The caller gives up in the send state while it waits for CSCON 1, the next transaction follows
 * at once: its SEND and the one the ready state posts for it are queued while there is no RRC
 * connection */
ZTEST(txn, test_give_up_in_send)
{
	stack_attach();
	go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* First transaction: no CSCON 1 */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Give up\"", "#XSEND: 7", "OK"},
		/* Second transaction: back in ready state, sent once, received after CSCON 1 */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		URC(500, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_SLEEP(5000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Give up",
		.send_len = 7,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(2),
		.rai = true,
	};

	int64_t elapsed;
	int ret = send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2000 && elapsed <= 2100, "returned %lld ms after the call",
		     elapsed);

	param.send_buf = "Hello world";
	param.send_len = 11;
	param.timeout = K_SECONDS(10);

	ret = send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	k_sleep(K_SECONDS(8));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_GIVE_UP_IN_XSEND)
/* The caller gives up while the modem has not answered AT#XSEND yet */
ZTEST(txn, test_give_up_in_xsend)
{
	stack_attach();
	go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\""},
		URC(1500, "#XSEND: 11"),
		URC(0, "OK"),
		/* Given up: no receive */
		URC(500, "+CSCON: 1"),
		ITEMS_SLEEP(5000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(1),
		.rai = true,
	};

	/* The call waits for the end of AT#XSEND */
	int64_t elapsed;
	int ret = send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 1550 && elapsed <= 1700, "returned %lld ms after the call",
		     elapsed);

	memset(recv_buf, CANARY, sizeof(recv_buf));
	recv_len = CANARY;

	k_sleep(K_SECONDS(8));

	assert_canary(recv_buf, sizeof(recv_buf), &recv_len);

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_LOCK_BUSY)
/* A second caller does not get the lock before its deadline */
ZTEST(txn, test_lock_busy)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Only the owner's transaction */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Owner\"", "#XSEND: 5", "OK"},
		{"AT#XRECV=5"},
		RX_DATA(2000, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_SLEEP(5000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char owner_recv_buf[20] = {0};
	size_t owner_recv_len = 0;
	struct ctr_lte_v2_send_recv_param owner_param = {
		.send_buf = "Owner",
		.send_len = 5,
		.send_as_string = true,
		.recv_buf = owner_recv_buf,
		.recv_size = sizeof(owner_recv_buf),
		.recv_len = &owner_recv_len,
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	caller_start(&owner_param, K_PRIO_PREEMPT(5));

	k_sleep(K_MSEC(200));

	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Busy",
		.send_len = 4,
		.send_as_string = true,
		.timeout = K_MSEC(500),
		.rai = true,
	};

	int64_t elapsed;
	int ret = send_recv(&param, &elapsed);
	zassert_equal(ret, -EBUSY, "expected -EBUSY but got: %d", ret);
	zassert_true(elapsed >= 500 && elapsed <= 600, "returned %lld ms after the call", elapsed);

	ret = caller_join(&elapsed);
	zassert_ok(ret, "owner's send_recv failed: %d", ret);
	zassert_equal(owner_recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(owner_recv_buf, "Hi there", owner_recv_len, "recv_buf incorrect");

	k_sleep(K_SECONDS(8));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

ZTEST_SUITE(txn, NULL, NULL, NULL, NULL, NULL);
