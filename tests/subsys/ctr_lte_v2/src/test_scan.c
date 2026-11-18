/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * The neighbour cell measurement on request ('lte ncellmeas'). Each scenario is a configuration of
 * its own (testcase.yaml) and runs from the attach.
 */

#include "hw_lines.h"
#include "stack.h"

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_state.h"

#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The bounds of the phases: RRC idle, measurement, the window for the URC after the stop */
#define T_IDLE_MS    60000
#define T_MEASURE_MS 180000
#define T_GRACE_MS   2000

/* The output of 'lte ncellmeas show' for HW_NCELLMEAS_REGISTERED, after its header line */
#define SHOW_REGISTERED                                                                            \
	"cell: 074FEB50, plmn: 23002, tac: 05F2, earfcn: 6300, pci: 488, rsrp: -73 dBm, serving\n" \
	"cell: 074FEB51, plmn: 23002, tac: 05F2, earfcn: 6300, pci: 339, rsrp: -75 dBm\n"          \
	"cell: 074FEB0C, plmn: 23001, tac: 3866, earfcn: 6200, pci: 14, rsrp: -74 dBm\n"           \
	"cell: 000AE5CA, plmn: 23003, tac: 8DCC, earfcn: 3544, pci: 135, rsrp: -65 dBm\n"          \
	"command succeeded\n"

#define SHOW_IN_PROGRESS "ncellmeas: in progress\ncommand succeeded\n"

static atomic_t m_cb_count;

static void cb_handler(struct ctr_lte_v2_cb *cb, enum ctr_lte_v2_cb_event event)
{
	if (event == CTR_LTE_V2_CB_EVENT_NCELLMEAS_DONE) {
		atomic_inc(&m_cb_count);
	}
}

static struct ctr_lte_v2_cb m_cb = {.handler = cb_handler};

/* Too big for the stack of the test thread */
static struct ctr_lte_v2_ncellmeas_param m_param;

static void assert_result(int result, int num_cells, bool valid)
{
	zassert_ok(ctr_lte_v2_get_ncellmeas_param(&m_param));
	zassert_equal(m_param.result, result, "expected result %d but got: %d", result,
		      m_param.result);
	zassert_equal(m_param.num_cells, num_cells, "expected %d cells but got: %d", num_cells,
		      m_param.num_cells);
	zassert_equal(m_param.valid, valid, "expected valid %d", valid);
}

/* The time of the script item within 1 s of the expected one */
static __maybe_unused void assert_tx_time(size_t index, int64_t expected)
{
	int64_t t = mock_ctr_lte_link_tx_time(index);

	zassert_true(t >= 0, "item %zu not reached", index);
	zassert_true(t >= expected - 1000 && t <= expected + 1000,
		     "item %zu at %lld ms, expected %lld ms", index, t, expected);
}

/* Start a script at a whole second, so the age the shell prints does not depend on the offset */
static __maybe_unused int64_t start_on_second(struct mock_link_item *items, size_t count)
{
	stack_sleep_until((k_uptime_get() / 1000 + 1) * 1000);
	mock_ctr_lte_link_start(items, count);

	return k_uptime_get();
}

/* The output of a shell command without the CRs and the empty lines before it */
static const char *shell_out(const char *cmd)
{
	static char buf[1024];
	const char *out = stack_shell_cmd(cmd);
	size_t len = 0;

	while (*out == '\r' || *out == '\n') {
		out++;
	}

	for (; *out && len < sizeof(buf) - 1; out++) {
		if (*out != '\r') {
			buf[len++] = *out;
		}
	}

	buf[len] = '\0';

	return buf;
}

/* The output of 'lte ncellmeas show' without the age in its first line */
static __maybe_unused void assert_show(const char *head, const char *body)
{
	const char *out = shell_out("lte ncellmeas show");
	const char *eol = strchr(out, '\n');

	zassert_not_null(eol, "%s", out);
	zassert_true(!strncmp(out, "ncellmeas: ", 11) && strstr(out, head) &&
			     strstr(out, head) < eol,
		     "expected head %s in: %s", head, out);
	zassert_str_equal(eol + 1, body, "%s", out);
}

static K_THREAD_STACK_DEFINE(m_waiter_stack, 1024);
static struct k_thread m_waiter_thread;
static int m_waiter_ret;

static void waiter_entry(void *p1, void *p2, void *p3)
{
	m_waiter_ret = ctr_lte_v2_wait_for_ncellmeas(K_FOREVER);
}

/* Wait for the measurement (K_FOREVER) in a thread of its own */
static __maybe_unused void waiter_start(void)
{
	m_waiter_ret = 1;
	k_thread_create(&m_waiter_thread, m_waiter_stack, K_THREAD_STACK_SIZEOF(m_waiter_stack),
			waiter_entry, NULL, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
}

static __maybe_unused int waiter_join(void)
{
	zassert_ok(k_thread_join(&m_waiter_thread, K_SECONDS(60)), "the waiter did not return");

	return m_waiter_ret;
}

#if defined(CONFIG_TEST_FEATURE_SCAN_NCELLMEAS)
/* From the ready state with the RRC connection open, on a network with PSM: the measurement waits
 * for the RRC idle, the modem announces the sleep again after the URC */
ZTEST(scan, test_ncellmeas)
{
	zassert_equal(ctr_lte_v2_schedule_ncellmeas(), -ENODEV);
	g_ctr_lte_v2_config.test = true;
	zassert_equal(ctr_lte_v2_schedule_ncellmeas(), -ENOTSUP);
	g_ctr_lte_v2_config.test = false;

	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* A command before the +CSCON: 0 is a violation */
		ITEMS_NCELLMEAS_PSM_READY(10000, 50000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	int64_t start = start_on_second(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());

	stack_sleep_until(start + 20000);
	ASSERT_FSM_STATE("ncellmeas");
	assert_tx_time(1, start + 10000);
	zassert_equal(ctr_lte_v2_schedule_ncellmeas(), -EALREADY);

	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(130)));
	assert_result(0, 4, true);
	assert_show("0s ago, ok\n", SHOW_REGISTERED);

	/* The sleep the modem announced after the URC */
	stack_wait_for_state("sleep", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 1);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_NCELLMEAS_FAIL)
/* Measurements in a row on the ready state: the ends without a URC, a late URC, a rejected
 * command, an unreadable URC, a dropped request, and the sleep announced during a measurement */
ZTEST(scan, test_ncellmeas_fail)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* 1. The RRC connection stays open: measured at the end of the idle bound */

	/* clang-format off */
	static struct mock_link_item items_idle[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(1000, "%NCELLMEAS: " HW_NCELLMEAS_CONNECTED),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_idle, ARRAY_SIZE(items_idle));
	int64_t start = k_uptime_get();

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(65)));
	assert_tx_time(0, start + T_IDLE_MS);
	assert_result(0, 1, true);
	stack_wait_for_state("ready", K_SECONDS(1));
	mock_ctr_lte_link_assert_done();

	/* 2. No URC: the stop gets the cells found so far; a transaction published meanwhile goes
	 * out only after the measurement. 3. A late URC changes nothing */

	/* clang-format off */
	static struct mock_link_item items_stop[] = {
		URC(5000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		/* Not at once, so that the FSM thread goes on after the OK first */
		{"AT%NCELLMEASSTOP", "OK"},
		URC(10, "%NCELLMEAS: " HW_NCELLMEAS_STOPPED),
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		URC(500, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		URC(5000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		URC(1000, "+CSCON: 0"),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_stop, ARRAY_SIZE(items_stop));
	start = k_uptime_get();

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());

	stack_sleep_until(start + 15000);
	ASSERT_FSM_STATE("ncellmeas");

	char recv_buf[20] = {0};
	size_t recv_len = 0;
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello",
		.send_len = 5,
		.send_as_string = true,
		.recv_buf = recv_buf,
		.recv_size = sizeof(recv_buf),
		.recv_len = &recv_len,
		.timeout = K_SECONDS(240),
		.rai = true,
	};

	stack_caller_start(&param, K_PRIO_PREEMPT(5));

	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(200)));
	assert_tx_time(2, mock_ctr_lte_link_tx_time(1) + T_MEASURE_MS);
	assert_result(-ETIMEDOUT, 3, true);
	zassert_equal(atomic_get(&m_cb_count), 2);

	int64_t elapsed;
	zassert_ok(stack_caller_join(&elapsed));
	zassert_equal(recv_len, 8, "recv_len incorrect");

	/* Past the late URC and the RRC release */
	k_sleep(K_SECONDS(8));
	mock_ctr_lte_link_assert_done();
	assert_result(-ETIMEDOUT, 3, true);
	zassert_equal(atomic_get(&m_cb_count), 2);
	ASSERT_FSM_STATE("ready");

	/* 4. The stop without a URC: the window after it ends empty */

	/* clang-format off */
	static struct mock_link_item items_no_urc[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		{"AT%NCELLMEASSTOP", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_no_urc, ARRAY_SIZE(items_no_urc));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(200)));
	zassert_true(k_uptime_get() >= mock_ctr_lte_link_tx_time(1) + T_GRACE_MS - 10,
		     "ended %lld ms after the stop", k_uptime_get() - mock_ctr_lte_link_tx_time(1));
	assert_result(-ETIMEDOUT, 0, false);
	mock_ctr_lte_link_assert_done();

	/* 5. Rejected: ends at once, no stop */

	/* clang-format off */
	static struct mock_link_item items_cme[] = {
		{"AT%NCELLMEAS=5,7", "+CME ERROR: 0"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cme, ARRAY_SIZE(items_cme));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(1)));
	assert_result(-EIO, 0, false);
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	ASSERT_FSM_STATE("ready");

	/* 6. A URC cut in the second cell: nothing of it is kept */

	/* clang-format off */
	static struct mock_link_item items_cut[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(1000, "%NCELLMEAS: " SYN_NCELLMEAS_CUT),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cut, ARRAY_SIZE(items_cut));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(5)));
	assert_result(-EBADMSG, 0, false);
	mock_ctr_lte_link_assert_done();

	/* 7. A request whose event the full ring dropped: the repeated request posts it again. The
	 * cooperative test thread does not let the FSM thread run until it sleeps */

	/* clang-format off */
	static struct mock_link_item items_again[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(1000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_again, ARRAY_SIZE(items_again));

	for (int i = 0; i < 16; i++) {
		ctr_lte_v2_gnss_set_enable(false);
	}

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	k_sleep(K_SECONDS(2));
	zassert_equal(mock_ctr_lte_link_tx_time(0), -1, "measured without the event");

	int64_t again = k_uptime_get();
	zassert_equal(ctr_lte_v2_schedule_ncellmeas(), -EALREADY);
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(5)));
	assert_tx_time(0, again + 500);
	assert_result(0, 4, true);
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 6);

	/* 8. The modem enters PSM during a measurement that gets no URC: the ready state gets the
	 * sleep announced in the measurement state */

	/* clang-format off */
	static struct mock_link_item items_psm[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(100, "%XMODEMSLEEP: 1,89999999"),
		{"AT%NCELLMEASSTOP", "OK"},
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_psm, ARRAY_SIZE(items_psm));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(200)));
	assert_result(-ETIMEDOUT, 0, false);

	stack_wait_for_state("sleep", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 7);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_NCELLMEAS_NO_PSM)

/* clang-format off */
/* The cell without PSM (U1): the ready state switches the modem off after an exchange */
#define CEREG_NO_PSM "+CEREG: 5,\"05F2\",\"074FEB01\",7,,,\"11100000\",\"11100000\""

#define ITEMS_CFUN4_SLEEP \
	{"AT#XSOCKET=0", "#XSOCKET: 0,\"closed\"", "OK"}, \
	{"AT+CFUN=4", "+CEREG: 0", "OK"}, \
	URC(0, "+CGEV: ME PDN DEACT 0"), \
	URC(0, "+CGEV: ME DETACH"), \
	URC(0, "%XSIM: 0"), \
	URC(0, "%XMODEMSLEEP: 4"), \
	{"AT#XSLEEP=2", "OK"}
/* clang-format on */

/* On a network without PSM the sleep state has the modem in CFUN=4: the request wakes it up, the
 * attach and the RRC idle come before the measurement, then the ready state switches it off */
ZTEST(scan, test_ncellmeas_no_psm)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items_send[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		URC(50, "%XMODEMSLEEP: 1,0"),
		URC(200, CEREG_NO_PSM),
		URC(400, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_CFUN4_SLEEP,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_send, ARRAY_SIZE(items_send));

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
	zassert_ok(stack_send_recv(&param, &elapsed));
	stack_wait_for_state("sleep", K_SECONDS(5));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Out of the flight mode, attach */
		{"AT+CFUN=1", "OK"},
		URC(500, "+CSCON: 1"),
		URC(50, CEREG_NO_PSM),
		{"AT+COPS?", "+COPS: 1,2,\"23002\",7", "OK"},
		{"AT+CEREG?", "+CEREG: 5," CEREG_NO_PSM, "OK"},
		{"AT%XCBAND", "%XCBAND: 20", "OK"},
		{"AT+CEINFO?", "+CEINFO: 0,1,C,8,1,-79,21", "OK"},
		{"AT+CGATT?", "+CGATT: 1", "OK"},
		{"AT+CGACT?", "+CGACT: 0,1", "OK"},
		{"AT+CGDCONT?", "+CGDCONT: 0,\"IP\",\"hardwario\",\"172.28.0.139\",0,0", "OK"},
		{"AT#XSOCKET=1,2,0,0", "#XSOCKET: 0,2,17", "OK"},
		ITEM_CONNECT,
		URC(1000, "#XCONNECT: 1"),
		URC(0, "OK"),
		ITEM_CONEVAL,
		/* The URC at once: the FSM has to be armed before the command */
		ITEMS_NCELLMEAS(5000, 0, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		ITEMS_CFUN4_SLEEP,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	const char *out = shell_out("lte ncellmeas show");
	zassert_str_equal(out, SHOW_IN_PROGRESS, "%s", out);

	/* Waits for the RRC idle */
	stack_wait_for_state("ncellmeas", K_SECONDS(15));
	out = shell_out("lte ncellmeas show");
	zassert_str_equal(out, SHOW_IN_PROGRESS, "%s", out);

	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(10)));
	assert_result(0, 4, true);

	stack_wait_for_state("sleep", K_SECONDS(5));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 1);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_NCELLMEAS_FROM_SLEEP)
/* From the sleep in PSM, in the order the board with PSM logged (gate 3A): back to sleep through
 * the sleep the modem announces after a URC, or through the FSM itself after an end without one */
ZTEST(scan, test_ncellmeas_from_sleep)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();
	stack_go_to_sleep();

	/* 1. A1: the URC after 118 s and the sleep announced again come in one batch */

	/* clang-format off */
	static struct mock_link_item items_a1[] = {
		ITEMS_NCELLMEAS_PSM(118000, "%NCELLMEAS: " HW_PSM_NCELLMEAS),
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_a1, ARRAY_SIZE(items_a1));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(125)));
	assert_result(0, 3, true);

	stack_wait_for_state("sleep", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 1);

	/* 2. Rejected: the modem does not announce the sleep again */

	/* clang-format off */
	static struct mock_link_item items_cme[] = {
		{"AT%NCELLMEAS=5,7", "+CME ERROR: 0"},
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cme, ARRAY_SIZE(items_cme));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(15)));
	assert_result(-EIO, 0, false);

	stack_wait_for_state("sleep", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 2);

	/* 3. A2: the stop gets the cells found so far, the sleep is announced after them */

	/* clang-format off */
	static struct mock_link_item items_a2[] = {
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(0, "%XMODEMSLEEP: 1,0"),
		{"AT%NCELLMEASSTOP", "OK"},
		URC(0, "%NCELLMEAS: " HW_PSM_NCELLMEAS_STOPPED),
		URC(0, "%XMODEMSLEEP: 1,86265318"),
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_a2, ARRAY_SIZE(items_a2));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(200)));
	assert_tx_time(2, mock_ctr_lte_link_tx_time(0) + T_MEASURE_MS);
	assert_result(-ETIMEDOUT, 1, true);

	stack_wait_for_state("sleep", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 3);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_NCELLMEAS_CANCEL)
/* The measurement ends early: the network is lost, LTE reconnects (also when the stop fails) */
ZTEST(scan, test_ncellmeas_cancel)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* 1. Lost network during the measurement: stopped, then the attach */

	/* clang-format off */
	static struct mock_link_item items_lost[] = {
		URC(1000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(5000, "+CEREG: 2,\"B4DC\",\"000AE520\",9"),
		{"AT%NCELLMEASSTOP", "OK"},
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_lost, ARRAY_SIZE(items_lost));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(10)));
	assert_result(-ECANCELED, 0, false);
	zassert_equal(atomic_get(&m_cb_count), 1);

	stack_wait_for_state("ready", K_SECONDS(10));
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();

	/* 2. Requested during the attach, LTE reconnects: the request is dropped */

	/* clang-format off */
	static struct mock_link_item items_drop[] = {
		URC(100, "+CEREG: 2,\"B4DC\",\"000AE520\",9"),
		/* The disabled state puts the modem to sleep, the boot attaches again */
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_drop, ARRAY_SIZE(items_drop));

	stack_wait_for_state("attach", K_SECONDS(2));
	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	waiter_start();
	k_sleep(K_MSEC(100));
	ASSERT_FSM_STATE("attach");

	stack_shell_cmd("lte reconnect");
	zassert_ok(waiter_join());

	stack_wait_for_state("open_socket", K_SECONDS(10));
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(6)));
	/* No measurement after the attach */
	k_sleep(K_SECONDS(2));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_cb_count), 1);

	/* 3. LTE reconnects during the measurement, the stop fails: disabled all the same */

	/* clang-format off */
	static struct mock_link_item items_reconnect[] = {
		URC(1000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		{"AT%NCELLMEASSTOP", "ERROR"},
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_reconnect, ARRAY_SIZE(items_reconnect));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	waiter_start();
	k_sleep(K_SECONDS(5));
	ASSERT_FSM_STATE("ncellmeas");

	stack_shell_cmd("lte reconnect");
	zassert_ok(waiter_join());
	assert_result(-ECANCELED, 0, false);
	zassert_equal(atomic_get(&m_cb_count), 2);

	stack_wait_for_state("open_socket", K_SECONDS(10));
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(6)));
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

ZTEST_SUITE(scan, NULL, NULL, NULL, NULL, NULL);
