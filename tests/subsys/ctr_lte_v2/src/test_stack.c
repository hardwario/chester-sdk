/* west twister --testsuite-root . -c -i -v -p native_posix */
/* west build -b native_posix -- -DCONFIG_TEST_FEATURE_STACK=y && ./build/zephyr/zephyr.elf */

/*
 * Test cases (run sequentially, depend on each other):
 *
 * - test_attach:          Initial connection to network
 * - test_send_only:       Send without receive (RAI_LAST)
 * - test_send_recv:       Send and receive with PSM
 * - test_timeout_in_send: API timeout before CSCON=1
 * - test_timeout_in_recv: API timeout during data wait
 * - test_send_recv_no_psm: Send and receive without PSM (CFUN=4 shutdown)
 */

#include "mock.h"

#include <chester/ctr_lte_v2.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>
#include <zephyr/settings/settings.h>

#include "ctr_lte_v2_state.h"

/* clang-format off */
#define URC(ms, urc) { .rx=urc, .delay_ms=ms }
#define RX_DATA(ms, urc, data) { .rx=urc, .rx2=data, .delay_ms=ms }
/* clang-format on */

#define ASSERT_FSM_STATE(expected)                                                                 \
	zassert_equal(strcmp(ctr_lte_v2_get_state(), expected), 0,                                 \
		      "expected state " expected " but got: %s", ctr_lte_v2_get_state())

/* ========================================================================== */
/* Test: Attach                                                               */
/* ========================================================================== */

static void test_attach(void)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		{"AT", "OK"},
		{"AT+CGSN=1", "+CGSN: \"351358815178345\"", "OK"},
		{"AT%HWVERSION", "%HWVERSION: nRF9160 SICA B1A", "OK"},
		{"AT%SHORTSWVER", "%SHORTSWVER: nrf9160_1.3.2", "OK"},
		{"AT#XSLMVER", "#XSLMVER: \"2.5.2\",\"2.5.0-lte-5ccd2d4dd54c\"", "OK"},
		{"AT#XVERSION", "#XVERSION: \"v1.7.0\"", "OK"},
		{"AT+CFUN=0", "OK"},
		{"AT%XPOFWARN=1,30", "OK"},
		{"AT%XTEMPHIGHLVL=70", "OK"},
		{"AT%XTEMP=1", "OK"},
		{"AT%XSYSTEMMODE=0,1,1,0", "OK"},
		{"AT%XDATAPRFL=0", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT%XBANDLOCK=1,\"0000000000000000000000100000000000000000000000000000000000001011000011110001100010011010\"", "OK"},
		#else
		{"AT%XBANDLOCK=0", "OK"},
		#endif
		{"AT%XSIM=1", "OK"},
		{"AT%XNETTIME=1", "OK"},
		{"AT%MDMEV=1", "OK"},
		{"AT%REL14FEAT=1,1,1,1,0", "OK"},
		{"AT%RAI=1", "OK"},
		{"AT+CPSMS=1,,,\"00111000\",\"00000000\"", "OK"},
		{"AT+CEPPI=1", "OK"},
		{"AT+CEREG=5", "OK"},
		{"AT+CGEREP=1", "OK"},
		{"AT+CMEE=1", "OK"},
		{"AT+CNEC=24", "OK"},
		{"AT%XCOEX0=1,1,1565,1586", "OK"},
		{"AT+CSCON=1", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT+COPS=1,2,\"23003\"", "OK"},
		{"AT+CGDCONT=0,\"IP\",\"hardwario\"", "OK"},
		#else
		{"AT+COPS=0", "OK"},
		{"AT+CGDCONT=0,\"IP\"", "OK"},
		#endif
		{"AT+CGAUTH=0,0", "OK"},
		{"AT%XMODEMSLEEP=1,500,10240", "OK"},
		{"AT+CFUN=1", "%XMODEMSLEEP: 4", "OK"},
		URC(0, "%XMODEMSLEEP: 4,0"),
		URC(0, "+CEREG: 0"),
		URC(500, "+CEREG: 2,\"B4DC\",\"000AE520\",9"),
		URC(100, "%XSIM: 1"),
		{"AT+CIMI", "901288910017273", "OK"},
		{"AT+CIMI", "901288910017273", "OK"},
		{"AT%XICCID", "%XICCID: 89882390000262826558", "OK"},
		{"AT+CRSM=176,28539,0,0,12", "+CRSM: 144,0,\"FFFFFFFFFFFFFFFFFFFFFFFF\"", "OK"},
		URC(100, "+CSCON: 1"),
		URC(50, "+CEREG: 5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\""),
		{"AT+COPS?", "+COPS: 1,2,\"23003\",9", "OK"},
		{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"", "OK"},
		{"AT%XCBAND", "%XCBAND: 20", "OK"},
		{"AT+CEINFO?", "+CEINFO: 0,1,C,8,1,-79,21", "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT+CGDCONT?", "+CGDCONT: 0,\"IP\",\"hardwario\",\"172.28.0.139\",0,0", "OK"},
		{"AT#XSOCKET=1,2,0,0", "#XSOCKET: 0,2,17", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT#XCONNECT=\"46.101.214.168\",5003", "#XCONNECT: 1", "OK"},
		#else
		{"AT#XCONNECT=\"192.168.192.4\",5003", "#XCONNECT: 1", "OK"},
		#endif
		{"AT%CONEVAL", "%CONEVAL: 0,1,7,95,30,49,\"000AE520\",\"23003\",135,6447,20,0,0,-8,2,1,73", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	ctr_lte_v2_enable();

	int ret = ctr_lte_v2_wait_for_connected(K_MSEC(6000));
	zassert_ok(ret, "ctr_lte_v2_wait_for_connected failed");

	k_sleep(K_MSEC(500));
}

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
}

/* ========================================================================== */
/* Test: Timeout helper (shared logic for timeout tests)                      */
/* ========================================================================== */

static void test_timeout_helper(k_timeout_t timeout)
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
		{"AT#XSEND=\"No response\"", "#XSEND: 11", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		URC(200, "%MDMEV: SEARCH STATUS 2"),
		URC(400, "+CSCON: 1"),
		/* Receive state - modem returns ERROR after 5s */
		{"AT#XRECV=5"},
		URC(5000, "ERROR"),
		/* Error state - flow check */
		{"AT+CFUN?", "+CFUN: 1", "OK"},
		{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"",
		 "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT#XSOCKET?", "#XSOCKET: 0,1,0,2,0", "OK"},
		/* Error state timeout (5s) -> ready state -> sleep */
		URC(6000, "+CSCON: 0"),
		URC(0, "%XMODEMSLEEP: 1,89999999"),
		{"AT#XSLEEP=2", "OK"},
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
		.timeout = timeout,
		.rai = true,
	};

	int ret = ctr_lte_v2_send_recv(&param);
	zassert_equal(ret, -EIO, "expected -EIO but got: %d", ret);

	k_sleep(K_MSEC(15000));

	ASSERT_FSM_STATE("sleep");
}

/* Timeout in send state (before CSCON=1, ~650ms into sequence) */
static void test_timeout_in_send(void)
{
	test_timeout_helper(K_MSEC(300));
}

/* Timeout in receive state (after CSCON=1, waiting for data) */
static void test_timeout_in_recv(void)
{
	test_timeout_helper(K_MSEC(2000));
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
}

/* ========================================================================== */
/* Main test - runs all subtests sequentially (they depend on each other)     */
/* ========================================================================== */

ZTEST(stack, test_full_sequence)
{
	test_attach();
	test_send_only();
	test_send_recv();
	test_timeout_in_send();
	test_timeout_in_recv();
	test_send_recv_no_psm();
}

ZTEST_SUITE(stack, NULL, NULL, NULL, NULL, NULL);
