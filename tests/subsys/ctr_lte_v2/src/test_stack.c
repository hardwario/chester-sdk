/* west twister --testsuite-root . -c -i -v -p native_sim */
/* west build -b native_sim -- -DCONFIG_TEST_FEATURE_STACK=y && ./build/zephyr/zephyr.elf */

/*
 * Test cases (run sequentially, depend on each other):
 *
 * - stack_attach:         Initial connection to network
 * - test_send_only:       Send without receive (RAI_LAST)
 * - test_send_recv:       Send and receive with PSM
 * - test_timeout_in_send: API timeout before CSCON=1, the FSM drops the send
 * - test_timeout_in_recv: API timeout during data wait, returns when the modem gives up
 * - test_send_recv_no_psm: Send and receive without PSM (CFUN=4 shutdown)
 */

#include "stack.h"

#include <chester/ctr_lte_v2.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>
#include <zephyr/settings/settings.h>

#include "ctr_lte_v2_state.h"

/* ========================================================================== */
/* Test: Send only (no receive)                                               */
/* ========================================================================== */

static void test_send_only(void)
{
	ASSERT_FSM_STATE("ready");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Flow check in sleep ready cscon=1 */
		{"AT+CFUN?", "+CFUN: 1", "OK"},
		{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"",
		 "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT#XSOCKET?", "#XSOCKET: 0,1,0,2,0", "OK"},
		{"AT#XSOCKETOPT=1,51", "OK"},  /* SO_RAI_LAST - send only, no response expected */
		{"AT#XSEND=\"hello\"", "#XSEND: 5", "OK"},
		{"AT#XSOCKETOPT=1,50", "OK"},  /* SO_RAI_NO_DATA - done sending */
		{"AT%CONEVAL",
		 "%CONEVAL: 0,1,7,95,30,49,\"000AE520\",\"23003\",135,6447,20,0,0,-8,2,1,73", "OK"},
		URC(100, "+CSCON: 0"),
		URC(0, "%XMODEMSLEEP: 1,89999999"),
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "hello",
		.send_len = 5,
		.send_as_string = true,
		.recv_buf = NULL,
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	int ret = ctr_lte_v2_send_recv(&param);
	zassert_ok(ret, "send_only failed: %d", ret);

	k_sleep(K_MSEC(5000));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}

/* ========================================================================== */
/* Test: Send and receive                                                     */
/* ========================================================================== */

static void test_send_recv(void)
{
	ASSERT_FSM_STATE("sleep");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Flow check in sleep state */
		{"AT+CFUN?", "+CFUN: 1", "OK"},
		{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"",
		 "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT#XSOCKET?", "#XSOCKET: 0,1,0,2,0", "OK"},
		/* Send state */
		{"AT#XSOCKETOPT=1,52", "OK"},  /* SO_RAI_ONE_RESP - expect response */
		{"AT#XSEND=\"Hello world\"", "#XSEND: 11", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		URC(200, "%MDMEV: SEARCH STATUS 2"),
		URC(500, "+CSCON: 1"),
		/* Receive state */
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},  /* SO_RAI_NO_DATA - done */
		/* Coneval state */
		{"AT%CONEVAL",
		 "%CONEVAL: 0,1,7,95,30,49,\"000AE520\",\"23003\",135,6447,20,0,0,-8,2,1,73", "OK"},
		/* Ready state -> later goes to sleep */
		URC(5000, "+CSCON: 0"),
		URC(0, "%XMODEMSLEEP: 1,89999999"),
		{"AT#XSLEEP=2", "OK"},
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

	int ret = ctr_lte_v2_send_recv(&param);
	zassert_ok(ret, "send_recv failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	k_sleep(K_MSEC(10000));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}

/* ========================================================================== */
/* Test: Timeouts                                                             */
/* ========================================================================== */

/* Timeout in send state (before CSCON=1, ~650ms into sequence) */
static void test_timeout_in_send(void)
{
	ASSERT_FSM_STATE("sleep");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Flow check in ready state */
		ITEMS_FLOW_CHECK,
		/* Send state */
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"No response\"", "#XSEND: 11", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		/* The caller gives up, the FSM is back in ready state */
		URC(200, "%MDMEV: SEARCH STATUS 2"),
		URC(400, "+CSCON: 1"),
		/* Ready state -> later goes to sleep */
		ITEMS_SLEEP(1000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "No response",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_MSEC(300),
		.rai = true,
	};

	int64_t start = k_uptime_get();
	int ret = ctr_lte_v2_send_recv(&param);
	int64_t elapsed = k_uptime_get() - start;
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed <= 400, "returned %lld ms after the call", elapsed);

	k_sleep(K_MSEC(15000));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}

/* Timeout in receive state (after CSCON=1, waiting for data) */
static void test_timeout_in_recv(void)
{
	ASSERT_FSM_STATE("sleep");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Flow check in ready state */
		ITEMS_FLOW_CHECK,
		/* Send state */
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"No response\"", "#XSEND: 11", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		URC(200, "%MDMEV: SEARCH STATUS 2"),
		URC(400, "+CSCON: 1"),
		/* Receive state - modem returns ERROR after 5s */
		{"AT#XRECV=5"},
		URC(5000, "ERROR"),
		/* No data within the receive window, no error state: on as after a response */
		ITEM_CONEVAL,
		/* Ready state -> sleep */
		ITEMS_SLEEP(6000),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "No response",
		.send_len = 11,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_MSEC(2000),
		.rai = true,
	};

	/* The caller waits until the modem gives the receive up */
	int64_t start = k_uptime_get();
	int ret = ctr_lte_v2_send_recv(&param);
	int64_t elapsed = k_uptime_get() - start;
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 5500 && elapsed <= 6000, "returned %lld ms after the call",
		     elapsed);

	k_sleep(K_MSEC(15000));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}

/* ========================================================================== */
/* Test: Send and receive (no PSM - network doesn't support PSM)              */
/* ========================================================================== */

static void test_send_recv_no_psm(void)
{
	ASSERT_FSM_STATE("sleep");

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Flow check in ready state */
		{"AT+CFUN?", "+CFUN: 1", "OK"},
		{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"",
		 "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT#XSOCKET?", "#XSOCKET: 0,1,0,2,0", "OK"},
		/* Send state */
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello no PSM\"", "#XSEND: 12", "OK"},
		/* Receive state */
		URC(50, "%XMODEMSLEEP: 1,0"),
		URC(200, "%MDMEV: SEARCH STATUS 2"),
		URC(200, "+CEREG: 5,\"05F2\",\"074FEB01\",7,,,\"11100000\",\"11100000\""),  /* PSM disabled */
		URC(400, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		/* Coneval state */
		{"AT%CONEVAL", "%CONEVAL: 0,1,7,62,11,27,\"074FEB01\",\"23002\",339,6300,20,0,0,3,1,1,94", "OK"},
		/* Ready state timeout -> no PSM, so CFUN=4 */
		{"AT#XSOCKET=0", "#XSOCKET: 0,\"closed\"", "OK"},
		{"AT+CFUN=4", "OK"},
		URC(0, "+CGEV: ME PDN DEACT 0"),
		URC(0, "+CEREG: 0"),
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
		.send_buf = "Hello no PSM",
		.send_len = 12,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(10),
		.rai = true,
	};

	int ret = ctr_lte_v2_send_recv(&param);
	zassert_ok(ret, "send_recv_no_psm failed: %d", ret);
	zassert_equal(recv_len, 8, "recv_len incorrect");
	zassert_mem_equal(recv_buf, "Hi there", recv_len, "recv_buf incorrect");

	k_sleep(K_MSEC(10000));

	ASSERT_FSM_STATE("sleep");
	mock_ctr_lte_link_assert_done();
}

/* The +CEREG URCs of the sequence went to the registration history */
static void test_cereg_history(void)
{
	/* One more than the URCs, to see that there are no others */
	struct ctr_lte_v2_cereg_event events[6];
	size_t count;

	BUILD_ASSERT(CONFIG_CTR_LTE_V2_CEREG_HISTORY >= 5);

	zassert_ok(ctr_lte_v2_get_cereg_history(events, ARRAY_SIZE(events), &count));
	zassert_equal(count, 5);

	/* Newest first: "+CEREG: 0" after CFUN=4 */
	zassert_equal(events[0].stat, CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED);

	/* The cell change during the receive */
	zassert_equal(events[1].stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_equal(events[1].tac, 0x05F2);
	zassert_equal(events[1].cid, 0x074FEB01);
	zassert_equal(events[1].act, CTR_LTE_V2_CEREG_PARAM_ACT_LTE);
	zassert_equal(events[1].reject_cause, 0);

	/* The attach, accepted with EMM cause 18 */
	zassert_equal(events[2].stat, CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING);
	zassert_equal(events[2].tac, 0xB4DC);
	zassert_equal(events[2].cid, 0x000AE520);
	zassert_equal(events[2].act, CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT);
	zassert_equal(events[2].reject_cause, 18);

	/* Searching, and "+CEREG: 0" after CFUN=1 */
	zassert_equal(events[3].stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING);
	zassert_equal(events[3].tac, 0xB4DC);
	zassert_equal(events[4].stat, CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED);

	/* which is not a reject */
	struct ctr_lte_v2_cereg_event reject;
	zassert_equal(ctr_lte_v2_get_last_reject(&reject), -ENODATA);
}

/* ========================================================================== */
/* Main test - runs all subtests sequentially (they depend on each other)     */
/* ========================================================================== */

ZTEST(stack, test_full_sequence)
{
	stack_attach();
	test_send_only();
	test_send_recv();
	test_timeout_in_send();
	test_timeout_in_recv();
	test_send_recv_no_psm();
	test_cereg_history();
}

static void match_thread_name(const struct k_thread *thread, void *user_data)
{
	const char *name = k_thread_name_get((k_tid_t)thread);
	bool *found = user_data;

	if (name && !strcmp(name, "ctr_lte_v2")) {
		*found = true;
	}
}

ZTEST(stack, test_work_queue_thread_name)
{
	bool found = false;

	k_thread_foreach(match_thread_name, &found);
	zassert_true(found, "no thread named ctr_lte_v2");
}

ZTEST_SUITE(stack, NULL, NULL, NULL, NULL, NULL);
