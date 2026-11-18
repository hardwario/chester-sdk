/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * The send_recv transaction when the caller gives up, LTE reconnects meanwhile or the network
 * grants no connection. Each scenario is a configuration of its own (testcase.yaml) and runs from
 * the attach.
 */

#include "stack.h"

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_state.h"

#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CANARY 0xa5

static __maybe_unused void assert_canary(const char *buf, size_t size, const size_t *len)
{
	for (size_t i = 0; i < size; i++) {
		zassert_equal((uint8_t)buf[i], CANARY, "recv_buf[%zu] written after the return", i);
	}

	zassert_equal(*len, CANARY, "recv_len written after the return");
}

/* Wait until the send state waits for CSCON 1 (after AT#XSEND) and return the time left on its
 * timer */
static __maybe_unused int send_timer_sec(void)
{
	int remaining_sec = 0;

	for (int i = 0; i < 1000; i++) {
		if (!strcmp(ctr_lte_v2_get_state(), "send")) {
			int ret = ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec);

			zassert_ok(ret, "attach info failed: %d", ret);
			if (remaining_sec > 0) {
				break;
			}
		}
		k_sleep(K_MSEC(10));
	}

	ASSERT_FSM_STATE("send");
	zassert_true(remaining_sec > 0, "no timer in the send state");

	return remaining_sec;
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
	stack_caller_start(&param, K_LOWEST_APPLICATION_THREAD_PRIO);

	int64_t elapsed;
	int ret = stack_caller_join(&elapsed);
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

	stack_give_up("attach");

	struct ctr_lte_v2_metrics metrics;
	zassert_ok(ctr_lte_v2_get_metrics(&metrics));
	zassert_equal(metrics.attach_fail_count, 0, "attach failed: %u", metrics.attach_fail_count);

	int remaining_sec;
	zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec));
	zassert_true(remaining_sec >= 300 - 3, "attach timer cut to %d s", remaining_sec);

	/* The attach times out at its time */
	stack_sleep_until(attach_start + 299 * MSEC_PER_SEC);
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

	stack_give_up("retry_delay");

	zassert_ok(ctr_lte_v2_get_curr_attach_info(NULL, NULL, NULL, &remaining_sec));
	zassert_true(remaining_sec >= 3600 - 4, "retry delay cut to %d s", remaining_sec);

	/* The retry delay runs to its end */
	stack_sleep_until(retry_delay_start + 3598 * MSEC_PER_SEC);
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
	stack_give_up("error");

	/* The error state ends at its time */
	stack_sleep_until(start + 4800);
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
	stack_go_to_sleep();

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
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2000 && elapsed <= 2100, "returned %lld ms after the call",
		     elapsed);

	param.send_buf = "Hello world";
	param.send_len = 11;
	param.timeout = K_SECONDS(10);

	ret = stack_send_recv(&param, &elapsed);
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
	stack_go_to_sleep();

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
	int ret = stack_send_recv(&param, &elapsed);
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

	stack_caller_start(&owner_param, K_PRIO_PREEMPT(5));

	k_sleep(K_MSEC(200));

	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Busy",
		.send_len = 4,
		.send_as_string = true,
		.timeout = K_MSEC(500),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -EBUSY, "expected -EBUSY but got: %d", ret);
	zassert_true(elapsed >= 500 && elapsed <= 600, "returned %lld ms after the call", elapsed);

	/* An invalid param is refused at once, before the lock */
	char recv_buf[8];
	struct ctr_lte_v2_send_recv_param invalid = {
		.send_buf = "Invalid",
		.send_len = 7,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = NULL,
		.timeout = K_MSEC(500),
		.rai = true,
	};

	ret = stack_send_recv(&invalid, &elapsed);
	zassert_equal(ret, -EINVAL, "expected -EINVAL but got: %d", ret);
	zassert_equal(elapsed, 0, "returned %lld ms after the call", elapsed);

	size_t recv_len = 0;
	invalid.recv_size = 0;
	invalid.recv_len = &recv_len;

	ret = stack_send_recv(&invalid, &elapsed);
	zassert_equal(ret, -EINVAL, "expected -EINVAL but got: %d", ret);
	zassert_equal(elapsed, 0, "returned %lld ms after the call", elapsed);

	zassert_equal(ctr_lte_v2_send_recv(NULL), -EINVAL);

	ret = stack_caller_join(&elapsed);
	zassert_ok(ret, "owner's send_recv failed: %d", ret);
	zassert_equal(owner_recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(owner_recv_buf, "Hi there", owner_recv_len, "recv_buf incorrect");

	k_sleep(K_SECONDS(8));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_CSCON_AGAIN_IN_RECV)
/* The network releases the connection and sets up a new one during AT#XRECV: AT%CONEVAL in that
 * moment wedges the modem (nRF9160 with SLM), so the FSM skips it for this exchange */
ZTEST(txn, test_cscon_again_in_recv)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		URC(200, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		URC(0, "+CSCON: 0"),
		URC(0, "+CSCON: 1"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		/* No AT%CONEVAL */
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
		.timeout = K_SECONDS(30),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");

	stack_wait_for_state("ready", K_MSEC(1000));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_CSCON_1_0_IN_SEND)
/* The network grants a connection for the datagram and releases it at once: CSCON 1 and CSCON 0
 * both arrive before the FSM handles CSCON 1, which still completes the transaction */
ZTEST(txn, test_cscon_1_0_in_send)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,51", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XSOCKETOPT=1,50", "OK"},
		URC(500, "+CSCON: 1"),
		URC(0, "+CSCON: 0"),
		ITEM_CONEVAL,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.timeout = K_SECONDS(30),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	/* With CSCON 1, not at the end of the wait for it */
	zassert_true(elapsed >= 500 && elapsed <= 1000, "returned %lld ms after the call", elapsed);

	stack_wait_for_state("ready", K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_RECONNECT_IN_RECV)
/* 'lte reconnect' during AT#XRECV: the FSM thread reconnects only after the dialog, the
 * transaction completes */
ZTEST(txn, test_reconnect_in_recv)
{
	/* Refused in the test mode and while LTE is disabled */
	g_ctr_lte_v2_config.test = true;
	zassert_equal(ctr_lte_v2_reconnect(), -ENOTSUP);
	g_ctr_lte_v2_config.test = false;

	zassert_equal(ctr_lte_v2_reconnect(), -ENODEV);
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("disabled");

	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		RX_DATA(1000, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		/* Only then the reconnect: the disabled state puts the modem to sleep, the boot
		 * attaches again */
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
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
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	stack_caller_start(&param, K_PRIO_PREEMPT(5));

	stack_wait_for_state("receive", K_SECONDS(2));
	/* Inside AT#XRECV */
	k_sleep(K_MSEC(300));

	zassert_ok(ctr_lte_v2_reconnect());

	/* Only requested, the FSM is still in the dialog */
	ASSERT_FSM_STATE("receive");

	int64_t elapsed;
	int ret = stack_caller_join(&elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	/* Attached again */
	stack_wait_for_state("open_socket", K_SECONDS(10));
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(6)));
	k_sleep(K_MSEC(500));

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_RECONNECT_IN_ERROR)
/* 'lte reconnect' in the error state shortly before its timer runs out: the timer does not fire
 * into the new boot */
ZTEST(txn, test_reconnect_in_error)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		/* The modem refuses the send */
		{"AT#XSOCKETOPT=1,51", "ERROR"},
		/* Error state - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Reconnect */
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	/* The send fails, the deadline comes in the error state */
	int64_t start = k_uptime_get();
	stack_give_up("error");

	/* The error timer would run out 150 ms after the reconnect: in the boot state, after the
	 * pause of the disabled state and before the modem wakes up */
	stack_sleep_until(start + 4850);
	ASSERT_FSM_STATE("error");

	zassert_ok(ctr_lte_v2_reconnect());

	/* Attached again */
	stack_wait_for_state("open_socket", K_SECONDS(10));
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(6)));
	k_sleep(K_MSEC(500));

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_RECV_NO_DATA)
/* No response within the receive window: the modem answers AT#XRECV with ERROR at its end. The
 * reply is lost, the socket is fine: -ETIMEDOUT, no error state and no second AT#XSEND */
ZTEST(txn, test_recv_no_data)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		URC(5000, "ERROR"),
		/* On as after a response */
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
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 5000 && elapsed <= 5200, "returned %lld ms after the call",
		     elapsed);
	zassert_equal(recv_len, 0, "recv_len written: %zu", recv_len);

	stack_wait_for_state("sleep", K_SECONDS(7));
	/* Past the pause after AT#XSLEEP */
	k_sleep(K_MSEC(500));

	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_RECV_FAIL)
/* AT#XRECV fails as no lost reply does: an ERROR early in the receive window comes from the
 * socket, and no answer at all is a talk timeout. Both end the transaction with -ETIMEDOUT and go
 * to the error state */
ZTEST(txn, test_recv_fail)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* First transaction: ERROR in the first half of the receive window */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		URC(2000, "ERROR"),
		/* Error state - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Second transaction: no answer to AT#XRECV */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello again\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		/* Error state after the talk timeout */
		ITEMS_FLOW_CHECK,
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
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2000 && elapsed <= 2200, "returned %lld ms after the call",
		     elapsed);
	stack_wait_for_state("error", K_MSEC(500));

	stack_wait_for_state("ready", K_SECONDS(6));

	param.send_buf = "Hello again";

	ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 30000 && elapsed <= 30200, "returned %lld ms after the call",
		     elapsed);
	stack_wait_for_state("error", K_MSEC(500));
	zassert_equal(recv_len, 0, "recv_len written: %zu", recv_len);

	/* Past the flow check of the error state */
	k_sleep(K_MSEC(100));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_ERROR_NO_SOCKET)
/* The socket is not a datagram client: the check before the send fails, and so does the one of
 * the error state, which ends the transaction with -ENOTCONN at once */
ZTEST(txn, test_error_no_socket)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Ready state: a datagram socket, but a server one */
		ITEMS_FLOW_CHECK_XSOCKET("#XSOCKET: 0,1,1,2,0"),
		/* Error state: a client socket, but a stream one; open again in 5 s */
		ITEMS_FLOW_CHECK_XSOCKET("#XSOCKET: 0,1,0,1,0"),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed <= 100, "returned %lld ms after the call", elapsed);

	ASSERT_FSM_STATE("error");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_DISABLED)
/* No transaction in the test mode, nor while LTE is disabled; entering the disabled state ends
 * the pending one */
ZTEST(txn, test_disabled)
{
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	int64_t elapsed;
	int ret;

	g_ctr_lte_v2_config.test = true;
	ret = stack_send_recv(&param, &elapsed);
	g_ctr_lte_v2_config.test = false;
	zassert_equal(ret, -ENOTSUP, "expected -ENOTSUP but got: %d", ret);
	zassert_equal(elapsed, 0, "returned %lld ms after the call", elapsed);

	/* Not enabled yet */
	ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed <= 100, "returned %lld ms after the call", elapsed);
	ASSERT_FSM_STATE("disabled");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* No network: the attach state */
		ITEMS_PREPARE,
		/* Reconnect */
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	ctr_lte_v2_enable();
	stack_wait_for_state("attach", K_SECONDS(10));

	/* Pending in the attach state, which leaves it to the ready state */
	stack_caller_start(&param, K_PRIO_PREEMPT(5));
	k_sleep(K_SECONDS(1));

	zassert_ok(ctr_lte_v2_reconnect());

	ret = stack_caller_join(&elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed >= 1000 && elapsed <= 1100, "returned %lld ms after the call",
		     elapsed);

	/* Attached again */
	stack_wait_for_state("open_socket", K_SECONDS(10));
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(6)));
	k_sleep(K_MSEC(500));

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_NO_CSCON)
/* The network grants no connection for the datagram: without a deadline, each AT#XSEND waits
 * 120 s for CSCON 1, and after 1 + retry_count of them the transaction ends with -ENOTCONN. The
 * extended error report of the modem (AT+CEER) is kept for the shell */
ZTEST(txn, test_no_cscon)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		/* No CSCON 1 within 120 s: back in the ready state, sent once more */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		/* Nor within the next 120 s: no third send, the cause the modem stored */
		{"AT+CEER", "+CEER: \"EMM 22\"", "OK"},
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
		.timeout = K_FOREVER,
		.rai = true,
		.retry_count = 1,
	};

	/* From a thread, so that a call that never returns fails the join */
	stack_caller_start(&param, K_PRIO_PREEMPT(5));
	k_sleep(K_SECONDS(239));

	int64_t elapsed;
	int ret = stack_caller_join(&elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed >= 240000 && elapsed <= 240200, "returned %lld ms after the call",
		     elapsed);
	zassert_equal(recv_len, 0, "recv_len written: %zu", recv_len);

	/* Past a third send */
	k_sleep(K_SECONDS(1));

	char ceer[CTR_LTE_V2_STATE_CEER_SIZE];
	zassert_ok(ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer)));
	zassert_str_equal(ceer, "\"EMM 22\"");

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_CSCON_TIMER)
/* The send state waits for CSCON 1 for the time left to the deadline of the transaction, at least
 * 5 s and at most 120 s */
ZTEST(txn, test_cscon_timer)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items_short[] = {
		/* No CSCON 1, the caller gives up */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Short\"", "#XSEND: 5", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_short, ARRAY_SIZE(items_short));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Short",
		.send_len = 5,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(3),
		.rai = true,
	};

	/* A deadline of 3 s: 5 s at least */
	stack_caller_start(&param, K_PRIO_PREEMPT(5));

	int timer_sec = send_timer_sec();
	zassert_true(timer_sec >= 4 && timer_sec <= 5, "timer of %d s", timer_sec);

	int64_t elapsed;
	int ret = stack_caller_join(&elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	stack_wait_for_state("ready", K_MSEC(100));
	mock_ctr_lte_link_assert_done();

	/* clang-format off */
	static struct mock_link_item items_late[] = {
		/* The modem refuses the send */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,51", "ERROR"},
		/* Error state - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Ready state: sent now, CSCON 1 a second later */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,51", "OK"},
		{"AT#XSEND=\"Late\"", "#XSEND: 4", "OK"},
		{"AT#XSOCKETOPT=1,50", "OK"},
		URC(1000, "+CSCON: 1"),
		ITEM_CONEVAL,
		ITEMS_SLEEP(2000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_late, ARRAY_SIZE(items_late));

	struct ctr_lte_v2_send_recv_param late_param = {
		.send_buf = "Late",
		.send_len = 4,
		.send_as_string = true,
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	/* Sent 5 s into a deadline of 60 s: the time left */
	stack_caller_start(&late_param, K_PRIO_PREEMPT(5));

	timer_sec = send_timer_sec();
	zassert_true(timer_sec >= 54 && timer_sec <= 55, "timer of %d s", timer_sec);

	ret = stack_caller_join(&elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	stack_wait_for_state("sleep", K_SECONDS(3));
	/* Past the pause after AT#XSLEEP */
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	/* clang-format off */
	static struct mock_link_item items_long[] = {
		/* No CSCON 1 */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Long\"", "#XSEND: 4", "OK"},
		/* A report longer than the one kept */
		{"AT+CEER", "+CEER: \"OTHER 0 and a text longer than the report kept\"", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_long, ARRAY_SIZE(items_long));

	param.send_buf = "Long";
	param.send_len = 4;
	param.timeout = K_SECONDS(300);

	/* A deadline of 300 s: 120 s at most, then the transaction ends */
	stack_caller_start(&param, K_PRIO_PREEMPT(5));

	timer_sec = send_timer_sec();
	zassert_true(timer_sec >= 119 && timer_sec <= 120, "timer of %d s", timer_sec);

	k_sleep(K_SECONDS(118));

	ret = stack_caller_join(&elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed >= 120000 && elapsed <= 120200, "returned %lld ms after the call",
		     elapsed);
	zassert_equal(recv_len, 0, "recv_len written: %zu", recv_len);

	stack_wait_for_state("ready", K_MSEC(100));
	mock_ctr_lte_link_assert_done();

	char ceer[CTR_LTE_V2_STATE_CEER_SIZE];
	zassert_ok(ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer)));
	zassert_str_equal(ceer, "\"OTHER 0 and a text longer than");
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_CSCON_0_IN_SEND)
/* CSCON 0 while the send state waits for CSCON 1: the modem has the datagram already, the wait
 * goes on without a second AT#XSEND */
ZTEST(txn, test_cscon_0_in_send)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		URC(300, "+CSCON: 0"),
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
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	k_sleep(K_SECONDS(8));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_ERROR_IN_SEND)
/* The send fails into the error state, whose flow check passes: the transaction goes on from the
 * ready state. A datagram the modem has not got is sent then; once the AT#XSEND line has gone
 * out, the datagram may be on its way, and with retry_count 0 the transaction ends with -ENOTCONN
 * instead of a second AT#XSEND */
ZTEST(txn, test_error_in_send)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* First transaction: the modem refuses the option before AT#XSEND */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "ERROR"},
		/* Error state - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Ready state: sent now, RRC still connected after the attach */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		/* Second transaction: no answer to AT#XSEND */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello again\""},
		/* Error state after the talk timeout - flow check, ready state in 5 s */
		ITEMS_FLOW_CHECK,
		/* Ready state: the check before the send, but no send; the modem has no cause */
		ITEMS_FLOW_CHECK,
		{"AT+CEER", "OK"},
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
		.timeout = K_SECONDS(60),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_true(elapsed >= 5000 && elapsed <= 5400, "returned %lld ms after the call",
		     elapsed);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");
	stack_wait_for_state("ready", K_MSEC(500));

	param.send_buf = "Hello again";
	recv_len = 0;

	ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ENOTCONN, "expected -ENOTCONN but got: %d", ret);
	zassert_true(elapsed >= 35000 && elapsed <= 35300, "returned %lld ms after the call",
		     elapsed);
	zassert_equal(recv_len, 0, "recv_len written: %zu", recv_len);

	stack_wait_for_state("ready", K_MSEC(100));
	/* Past a second send */
	k_sleep(K_SECONDS(1));
	mock_ctr_lte_link_assert_done();

	char ceer[CTR_LTE_V2_STATE_CEER_SIZE];
	zassert_equal(ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer)), -ENODATA);
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_REATTACH_IN_SEND)
/* The network deregisters while the send state waits for CSCON 1: the socket the attach opens
 * again has not got the datagram, which is sent once more */
ZTEST(txn, test_reattach_in_send)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		/* Deregistered: attach, open the socket */
		URC(500, "+CEREG: 2,\"B4DC\",\"000AE520\",9"),
		ITEMS_REGISTER,
		/* Ready state: sent again, RRC connected */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
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
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(30),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	k_sleep(K_SECONDS(8));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_DEREGISTERED_IN_READY)
/* The network is lost with no cell left ("+CEREG: 2" without a TAC) while the FSM is ready: it
 * attaches again and opens the socket */
ZTEST(txn, test_deregistered_in_ready)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		URC(100, "+CEREG: 2"),
		/* Attach: registered again, open the socket */
		ITEMS_REGISTER,
		ITEMS_SLEEP(500),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	stack_wait_for_state("attach", K_SECONDS(2));
	stack_wait_for_state("sleep", K_SECONDS(10));
	/* Past the pause after AT#XSLEEP */
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	/* The loss is in the registration history, one more than the URCs to see that there are
	 * no others */
	struct ctr_lte_v2_cereg_event events[6];
	size_t count;

	BUILD_ASSERT(CONFIG_CTR_LTE_V2_CEREG_HISTORY >= 5);

	zassert_ok(ctr_lte_v2_get_cereg_history(events, ARRAY_SIZE(events), &count));
	zassert_equal(count, 5);

	/* Newest first: registered again */
	zassert_equal(events[0].stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_equal(events[0].tac, 0xB4DC);

	/* The loss, without a cell */
	zassert_equal(events[1].stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING);
	zassert_equal(events[1].tac, 0);
	zassert_equal(events[1].cid, 0);

	/* The first attach: registered, searching, and "+CEREG: 0" after CFUN=1 */
	zassert_equal(events[2].stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_equal(events[3].stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING);
	zassert_equal(events[3].tac, 0xB4DC);
	zassert_equal(events[4].stat, CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED);
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_NO_PSM_CFUN4)
/* On a cell without PSM the ready state switches the modem off (CFUN=4) after an exchange: the
 * values of the registration stay. After the wake-up a loss of the network is stored again */
ZTEST(txn, test_no_psm_cfun4)
{
	stack_attach();
	stack_go_to_sleep();

	/* The attach granted PSM */
	const char *out = stack_shell_cmd("lte state");
	zassert_not_null(strstr(out, "psm: active-time 0s, periodic-tau 1d 00h 00m 00s"), "%s",
			 out);

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		/* A cell without PSM */
		URC(200, "+CEREG: 5,\"05F2\",\"074FEB01\",7,,,\"11100000\",\"11100000\""),
		URC(400, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		/* Ready state timeout: no PSM, so CFUN=4; the URC can come before the OK */
		{"AT#XSOCKET=0", "#XSOCKET: 0,\"closed\"", "OK"},
		{"AT+CFUN=4", "+CEREG: 0", "OK"},
		URC(0, "+CGEV: ME PDN DEACT 0"),
		URC(0, "+CGEV: ME DETACH"),
		URC(0, "+CSCON: 0"),
		URC(0, "%XSIM: 0"),
		URC(0, "%XMODEMSLEEP: 4"),
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello",
		.send_len = 5,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_ok(ret, "send_recv failed: %d", ret);

	stack_wait_for_state("sleep", K_SECONDS(5));
	/* Past the pause after AT#XSLEEP */
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	/* The values of the registration, not the "+CEREG: 0" of the CFUN=4 */
	struct ctr_lte_v2_cereg_param cereg;
	zassert_ok(ctr_lte_v2_get_cereg_param(&cereg));
	zassert_equal(cereg.stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_str_equal(cereg.tac, "05F2");
	zassert_equal(cereg.active_time, -1);

	out = stack_shell_cmd("lte state");
	zassert_not_null(strstr(out, "cereg: registered-roaming"), "%s", out);
	zassert_not_null(strstr(out, "psm: not granted"), "%s", out);

	/* The history has it */
	struct ctr_lte_v2_cereg_event event;
	size_t count;
	zassert_ok(ctr_lte_v2_get_cereg_history(&event, 1, &count));
	zassert_equal(count, 1);
	zassert_equal(event.stat, CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED);

	/* clang-format off */
	static struct mock_link_item items_wake_up[] = {
		/* Out of the flight mode, attach */
		{"AT+CFUN=1", "OK"},
		/* The network is lost */
		URC(200, "+CEREG: 2"),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_wake_up, ARRAY_SIZE(items_wake_up));

	/* The send wakes the modem up, the attach does not end before the deadline */
	stack_give_up("attach");
	mock_ctr_lte_link_assert_done();

	zassert_ok(ctr_lte_v2_get_cereg_param(&cereg));
	zassert_equal(cereg.stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING);

	/* No PSM line without a registration */
	out = stack_shell_cmd("lte state");
	zassert_not_null(strstr(out, "cereg: searching"), "%s", out);
	zassert_is_null(strstr(out, "psm:"), "%s", out);
}
#endif

#if defined(CONFIG_TEST_FEATURE_TXN_CALLBACKS)
static atomic_t m_cb_count;
static atomic_t m_cb_once_count;
static enum ctr_lte_v2_cb_event m_cb_events[4];
static void *m_cb_user_data;

static void cb_handler(struct ctr_lte_v2_cb *cb, enum ctr_lte_v2_cb_event event)
{
	atomic_val_t i = atomic_inc(&m_cb_count);

	if (i < ARRAY_SIZE(m_cb_events)) {
		m_cb_events[i] = event;
	}

	m_cb_user_data = cb->user_data;
}

/* Removes itself on the first event */
static void cb_once_handler(struct ctr_lte_v2_cb *cb, enum ctr_lte_v2_cb_event event)
{
	atomic_inc(&m_cb_once_count);
	ctr_lte_v2_remove_callback(cb);
}

/* The callbacks get CSCON 1 of the attach and CSCON 0 before the sleep */
ZTEST(txn, test_callbacks)
{
	static int user_data;
	static struct ctr_lte_v2_cb cb = {.handler = cb_handler, .user_data = &user_data};
	static struct ctr_lte_v2_cb once = {.handler = cb_once_handler};
	static struct ctr_lte_v2_cb no_handler;

	zassert_equal(ctr_lte_v2_add_callback(NULL), -EINVAL);
	zassert_equal(ctr_lte_v2_add_callback(&no_handler), -EINVAL);
	zassert_equal(ctr_lte_v2_remove_callback(NULL), -EINVAL);

	/* Ahead of cb: removing itself must not stop the others */
	zassert_ok(ctr_lte_v2_add_callback(&once));
	zassert_ok(ctr_lte_v2_add_callback(&cb));
	zassert_equal(ctr_lte_v2_add_callback(&cb), -EALREADY);

	stack_attach();
	stack_go_to_sleep();

	zassert_equal(atomic_get(&m_cb_count), 2);
	zassert_equal(m_cb_events[0], CTR_LTE_V2_CB_EVENT_CSCON_1);
	zassert_equal(m_cb_events[1], CTR_LTE_V2_CB_EVENT_CSCON_0);
	zassert_equal_ptr(m_cb_user_data, &user_data);

	zassert_equal(atomic_get(&m_cb_once_count), 1, "not removed by its handler");
	zassert_equal(ctr_lte_v2_remove_callback(&once), -ENOENT);

	zassert_ok(ctr_lte_v2_remove_callback(&cb));
	zassert_equal(ctr_lte_v2_remove_callback(&cb), -ENOENT);
}
#endif

ZTEST_SUITE(txn, NULL, NULL, NULL, NULL, NULL);
