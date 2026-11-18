/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * The neighbour cell measurement on request ('lte ncellmeas') and the network scan ('lte scan').
 * Each scenario is a configuration of its own (testcase.yaml) and runs from the attach.
 */

#include "hw_lines.h"
#include "stack.h"

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_state.h"

#include <chester/ctr_lte_v2.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bounds of the phases: RRC idle, measurement, the window for the URC after the stop */
#define T_IDLE_MS    60000
#define T_MEASURE_MS 180000
#define T_GRACE_MS   2000

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

/* The registration without PSM after the CFUN=1, to the ready state */
#define ITEMS_REGISTER_NO_PSM \
	URC(500, "+CSCON: 1"), \
	URC(50, CEREG_NO_PSM), \
	{"AT+COPS?", "+COPS: 1,2,\"23002\",7", "OK"}, \
	{"AT+CEREG?", "+CEREG: 5," CEREG_NO_PSM, "OK"}, \
	{"AT%XCBAND", "%XCBAND: 20", "OK"}, \
	{"AT+CEINFO?", "+CEINFO: 0,1,C,8,1,-79,21", "OK"}, \
	{"AT+CGATT?", "+CGATT: 1", "OK"}, \
	{"AT+CGACT?", "+CGACT: 0,1", "OK"}, \
	{"AT+CGDCONT?", "+CGDCONT: 0,\"IP\",\"hardwario\",\"172.28.0.139\",0,0", "OK"}, \
	{"AT#XSOCKET=1,2,0,0", "#XSOCKET: 0,2,17", "OK"}, \
	ITEM_CONNECT, \
	URC(1000, "#XCONNECT: 1"), \
	URC(0, "OK"), \
	ITEM_CONEVAL
/* clang-format on */

/* The output of 'lte ncellmeas show' for HW_NCELLMEAS_REGISTERED, after its header line */
#define SHOW_REGISTERED                                                                            \
	"cell: 074FEB50, plmn: 23002, tac: 05F2, earfcn: 6300, pci: 488, rsrp: -74 dBm, serving\n" \
	"cell: 074FEB51, plmn: 23002, tac: 05F2, earfcn: 6300, pci: 339, rsrp: -76 dBm\n"          \
	"cell: 074FEB0C, plmn: 23001, tac: 3866, earfcn: 6200, pci: 14, rsrp: -75 dBm\n"           \
	"cell: 000AE5CA, plmn: 23003, tac: 8DCC, earfcn: 3544, pci: 135, rsrp: -66 dBm\n"          \
	"command succeeded\n"

#define SHOW_IN_PROGRESS "ncellmeas: in progress\ncommand succeeded\n"

static atomic_t m_cb_count;
static atomic_t m_scan_cb_count;

static void cb_handler(struct ctr_lte_v2_cb *cb, enum ctr_lte_v2_cb_event event)
{
	if (event == CTR_LTE_V2_CB_EVENT_NCELLMEAS_DONE) {
		atomic_inc(&m_cb_count);
	} else if (event == CTR_LTE_V2_CB_EVENT_SCAN_DONE) {
		atomic_inc(&m_scan_cb_count);
	}
}

static __maybe_unused struct ctr_lte_v2_cb m_cb = {.handler = cb_handler};

/* Too big for the stack of the test thread */
static struct ctr_lte_v2_ncellmeas_param m_param;

static __maybe_unused void assert_result(int result, int num_cells, bool valid)
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

/* The output of a shell command without the CRs, the colors (shell_info) and the empty lines
 * before it */
static __maybe_unused const char *shell_clean(const char *out)
{
	static char buf[2048];
	size_t len = 0;

	while (*out == '\r' || *out == '\n') {
		out++;
	}

	for (; *out && len < sizeof(buf) - 1; out++) {
		if (*out == '\x1b' && out[1] == '[') {
			/* ESC [ <parameters> <letter>: a color, or the cursor (help) */
			for (out += 2; *out && !isalpha((unsigned char)*out);) {
				out++;
			}
			if (!*out) {
				break;
			}
		} else if (*out != '\r') {
			buf[len++] = *out;
		}
	}

	buf[len] = '\0';

	return buf;
}

static __maybe_unused const char *shell_out(const char *cmd)
{
	return shell_clean(stack_shell_cmd(cmd));
}

static K_THREAD_STACK_DEFINE(m_shell_stack, 4096);
static struct k_thread m_shell_thread;
static int m_shell_ret;

static void shell_entry(void *p1, void *p2, void *p3)
{
	m_shell_ret = shell_execute_cmd(shell_backend_dummy_get_ptr(), p1);
}

/* A shell command in a thread of its own: the test goes on meanwhile (without the shell) */
static __maybe_unused void shell_bg_start(const char *cmd)
{
	const struct shell *sh = shell_backend_dummy_get_ptr();

	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
	shell_backend_dummy_clear_output(sh);
	k_thread_create(&m_shell_thread, m_shell_stack, K_THREAD_STACK_SIZEOF(m_shell_stack),
			shell_entry, (void *)cmd, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
}

/* Its return code and output (as shell_out) */
static __maybe_unused const char *shell_bg_join(int *ret, k_timeout_t timeout)
{
	size_t size;

	zassert_ok(k_thread_join(&m_shell_thread, timeout), "the command did not return");
	*ret = m_shell_ret;

	return shell_clean(shell_backend_dummy_get_output(shell_backend_dummy_get_ptr(), &size));
}

/* The output with each age before " ago" (back to the colon) replaced by "*" */
static __maybe_unused const char *no_ages(const char *out)
{
	static char buf[2048];
	size_t len = 0;

	for (; *out && len < sizeof(buf) - 3; out++) {
		if (!strncmp(out, " ago", 4)) {
			while (len > 0 && buf[len - 1] != ':') {
				len--;
			}
			buf[len++] = ' ';
			buf[len++] = '*';
		}
		buf[len++] = *out;
	}

	buf[len] = '\0';

	return buf;
}

/* The output of 'lte scan now': the first line, the progress every 30 s (n times), the result */
static __maybe_unused const char *scan_now_text(const char *first, int n, const char *result)
{
	static char buf[2048];
	size_t len = snprintf(buf, sizeof(buf), "%sscanning, this takes minutes...\n", first);

	for (int i = 1; i <= n; i++) {
		char took[24];

		ctr_util_fmt_duration(30 * i, took, sizeof(took));
		len += snprintf(buf + len, sizeof(buf) - len, "still scanning (%s)\n", took);
	}

	snprintf(buf + len, sizeof(buf) - len, "%scommand succeeded\n", result);

	return buf;
}

/* A shell command that fails with ret and prints the text */
static __maybe_unused void assert_shell_fails(const char *cmd, int ret, const char *text)
{
	const struct shell *sh = shell_backend_dummy_get_ptr();
	size_t size;

	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
	shell_backend_dummy_clear_output(sh);
	int got = shell_execute_cmd(sh, cmd);
	const char *out = shell_backend_dummy_get_output(sh, &size);

	zassert_equal(got, ret, "%s: expected %d but got: %d: %s", cmd, ret, got, out);
	zassert_not_null(strstr(out, text), "%s: expected %s in: %s", cmd, text, out);
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
	int (*wait)(k_timeout_t timeout) = p1;

	m_waiter_ret = wait(K_FOREVER);
}

/* Wait for the measurement or the scan (K_FOREVER) in a thread of its own */
static __maybe_unused void waiter_start(int (*wait)(k_timeout_t timeout))
{
	m_waiter_ret = 1;
	k_thread_create(&m_waiter_thread, m_waiter_stack, K_THREAD_STACK_SIZEOF(m_waiter_stack),
			waiter_entry, wait, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
}

static __maybe_unused int waiter_join(void)
{
	zassert_ok(k_thread_join(&m_waiter_thread, K_SECONDS(60)), "the waiter did not return");

	return m_waiter_ret;
}

/* The bounds of the scan steps: the registration, AT+COPS=?, and of the AT+CFUN=0 before a reset */
#define T_REGISTER_MS 120000
#define T_COPS_MS     300000
#define T_CFUN0_MS    120000

/* clang-format off */
/* The registration on LTE-M (U1, without PSM) and on NB-IoT with PSM (the board with PSM) */
#define ITEMS_SCAN_REGISTER_LTEM \
	URC(2000, "+CSCON: 1"), \
	URC(50, CEREG_NO_PSM)

#define ITEMS_SCAN_REGISTER_PSM \
	URC(2000, "+CSCON: 1"), \
	URC(50, "+CEREG: 5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"")

/* The cells and the networks of LTE-M as the U1 logged them (HW rounds 1 and 2) */
/* The same in PSM (the board with PSM): ITEMS_NCELLMEAS_PSM_READY up to the URC. AT+COPS=? follows
 * the URC at once, so the sleep the modem announces 77 ms after it falls into the search; another
 * one comes after the OK (gate 3A, A3) */
#define ITEMS_SCAN_CELLS_COPS_PSM \
	URC(10000, "+CSCON: 0"), \
	{"AT%NCELLMEAS=5,7", "OK"}, \
	URC(0, "%XMODEMSLEEP: 1,86399601"), \
	URC(0, "%XMODEMSLEEP: 1,0"), \
	URC(118000, "%NCELLMEAS: " HW_PSM_NCELLMEAS), \
	{"AT+COPS=?"}, \
	URC(77, "%XMODEMSLEEP: 1,86280784"), \
	URC(114000, "+COPS: " HW_COPS_PSM_NBIOT), \
	URC(0, "OK"), \
	URC(0, "%XMODEMSLEEP: 1,0")

#define ITEMS_SCAN_CELLS_COPS_LTEM \
	ITEMS_NCELLMEAS(10000, 50000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED), \
	ITEMS_SCAN_COPS(74000, HW_COPS_LIST)

/* NB-IoT camped without a registration (U1): measured at the end of the registration bound */
#define ITEMS_SCAN_NBIOT_CAMPED \
	ITEMS_SCAN_RAT_START("0,1,1,0"), \
	URC(17000, HW_CEREG_NBIOT_CAMPED), \
	{"AT%NCELLMEAS=5,7", "OK"}, \
	URC(14000, "%NCELLMEAS: " HW_NCELLMEAS_NBIOT_CAMPED)

/* A transaction from the ready state with the RRC connection open */
#define ITEMS_TXN \
	ITEMS_FLOW_CHECK, \
	{"AT#XSOCKETOPT=1,52", "OK"}, \
	{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"}, \
	{"AT#XRECV=5"}, \
	RX_DATA(200, "#XRECV: 8", "Hi there"), \
	{"AT#XSOCKETOPT=1,50", "OK"}, \
	ITEM_CONEVAL
/* clang-format on */

/* Too big for the stack of the test thread */
static struct ctr_lte_v2_scan_result m_scan;

static char m_recv_buf[20];
static size_t m_recv_len;

/* A transaction with a response, and one that waits for ever (send only) */
static __maybe_unused struct ctr_lte_v2_send_recv_param m_txn = {
	.send_buf = "Hello",
	.send_len = 5,
	.send_as_string = true,
	.recv_buf = m_recv_buf,
	.recv_size = sizeof(m_recv_buf),
	.recv_len = &m_recv_len,
	.timeout = K_SECONDS(10),
	.rai = true,
};

static __maybe_unused struct ctr_lte_v2_send_recv_param m_txn_forever = {
	.send_buf = "Hello",
	.send_len = 5,
	.send_as_string = true,
	.timeout = K_FOREVER,
	.rai = true,
};

/* Index of the n-th item (from 1) that sends the line, or of the n-th URC starting with it */
static __maybe_unused size_t item_index(const struct mock_link_item *items, size_t count, bool urc,
					const char *line, int n)
{
	for (size_t i = 0; i < count; i++) {
		const char *s = urc ? (items[i].tx ? NULL : items[i].rx) : items[i].tx;

		if (s && (urc ? !strncmp(s, line, strlen(line)) : !strcmp(s, line)) && !--n) {
			return i;
		}
	}

	zassert_unreachable("no item %s", line);

	return 0;
}

#define TX_INDEX(items, tx, n)  item_index(items, ARRAY_SIZE(items), false, tx, n)
#define URC_INDEX(items, rx, n) item_index(items, ARRAY_SIZE(items), true, rx, n)
#define TX_TIME(items, tx, n)   mock_ctr_lte_link_tx_time(TX_INDEX(items, tx, n))
#define URC_TIME(items, rx, n)  mock_ctr_lte_link_tx_time(URC_INDEX(items, rx, n))

/* The ended scan: the step results of LTE-M and NB-IoT, the cells, the networks in the lists */
static __maybe_unused void assert_scan(int reg1, int cells1, int nets1, int reg2, int cells2,
				       int nets2, int cell_count, int network_count)
{
	zassert_ok(ctr_lte_v2_get_scan_result(&m_scan));
	zassert_true(m_scan.valid, "expected an ended scan");

	const int got[] = {m_scan.lte_m.registration,  m_scan.lte_m.cells,  m_scan.lte_m.networks,
			   m_scan.nb_iot.registration, m_scan.nb_iot.cells, m_scan.nb_iot.networks};
	const int exp[] = {reg1, cells1, nets1, reg2, cells2, nets2};

	for (size_t i = 0; i < ARRAY_SIZE(exp); i++) {
		zassert_equal(got[i], exp[i],
			      "step %zu (lte-m 0-2, nb-iot 3-5): expected %d but got: %d", i,
			      exp[i], got[i]);
	}

	zassert_equal(m_scan.cell_count, cell_count, "expected %d cells but got: %d", cell_count,
		      m_scan.cell_count);
	zassert_equal(m_scan.network_count, network_count, "expected %d networks but got: %d",
		      network_count, m_scan.network_count);
}

/* After the restore: connected again, the ready state, the script done */
static __maybe_unused void assert_restored(void)
{
	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(60)));
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}

/* The n-th reset of the script came right before the item, and the wake-up right after it */
static __maybe_unused void assert_reset_at(int n, size_t index)
{
	int got = mock_ctr_lte_link_reset_item(n);

	zassert_equal(got, (int)index, "reset %d: expected before item %zu but got: %d", n, index,
		      got);
}

static __maybe_unused void txn_ok(void)
{
	int64_t elapsed;

	zassert_ok(stack_send_recv(&m_txn, &elapsed));
	zassert_equal(m_recv_len, 8);
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
	waiter_start(ctr_lte_v2_wait_for_ncellmeas);
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
	waiter_start(ctr_lte_v2_wait_for_ncellmeas);
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

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_BOTH)
/* Both RATs from the ready state, as the U1 logged them: LTE-M registers, NB-IoT only camps */
ZTEST(scan, test_scan_both)
{
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL), -ENODEV);
	g_ctr_lte_v2_config.test = true;
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL), -ENOTSUP);
	g_ctr_lte_v2_config.test = false;

	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		/* A command before the +CSCON: 0 is a violation */
		ITEMS_NCELLMEAS(10000, 50000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		{"AT+COPS=?"},
		/* Counts only in the registration step */
		URC(30000, SYN_CEREG_STALE_LTEM),
		URC(44000, "+COPS: " HW_COPS_LIST),
		URC(0, "OK"),
		ITEM_SCAN_CFUN0,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		URC(17000, HW_CEREG_NBIOT_CAMPED),
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(14000, "%NCELLMEAS: " HW_NCELLMEAS_NBIOT_CAMPED),
		ITEMS_SCAN_COPS(140000, HW_COPS_NBIOT_CAMPED),
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	int resets = mock_ctr_lte_link_reset_count();

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	k_sleep(K_SECONDS(1));
	ASSERT_FSM_STATE("scan");
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL), -EALREADY);
	zassert_equal(ctr_lte_v2_schedule_ncellmeas(), -EBUSY);
	assert_shell_fails("lte ncellmeas", -EBUSY, "scan in progress");
	zassert_equal(ctr_lte_v2_wait_for_connected(K_NO_WAIT), -ETIMEDOUT);
	zassert_equal(ctr_lte_v2_wait_for_scan(K_NO_WAIT), -ETIMEDOUT);

	/* While it runs, the previous result: none yet */
	const char *out = shell_out("lte scan show");
	zassert_str_equal(out, "no scan yet\n", "%s", out);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	/* Published before the restore */
	ASSERT_FSM_STATE("scan");

	assert_tx_time(TX_INDEX(items, "AT+COPS=?", 1),
		       URC_TIME(items, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED, 1));
	assert_tx_time(TX_INDEX(items, "AT%NCELLMEAS=5,7", 2),
		       TX_TIME(items, "AT+CFUN=1", 2) + T_REGISTER_MS);

	assert_scan(0, 0, 0, -ETIMEDOUT, 0, 0, 5, 6);
	/* Each RAT starts by a reset */
	zassert_equal(mock_ctr_lte_link_reset_count() - resets, 2);
	zassert_equal(m_scan.cells[0].act, 7);
	zassert_equal(m_scan.cells[0].eci, 0x074FEB50);
	zassert_true(m_scan.cells[0].serving);
	zassert_equal(m_scan.cells[4].act, 9);
	zassert_equal(m_scan.cells[4].eci, 0x074FEBAC);
	zassert_str_equal(m_scan.networks[2].plmn, "23002");
	zassert_equal(m_scan.networks[2].stat, 2);
	zassert_equal(m_scan.networks[2].act, 7);
	zassert_str_equal(m_scan.networks[4].plmn, "23001");
	zassert_equal(m_scan.networks[4].act, 9);
	zassert_equal(m_scan.mode, CTR_LTE_V2_SCAN_ALL);
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	/* By PLMN, the cells of a PLMN LTE-M first, then the stronger first. The age is from the
	 * end */
	out = shell_out("lte scan show");
	zassert_str_equal(
		out,
		"last scan: 0s ago (request)\n"
		"search: lte-m cells ok, nb-iot cells ok, networks ok\n"
		"plmn: 23001, act: lte-m,nb-iot, stat: available (lte-m), current (nb-iot)\n"
		"  cell: 074FEB0C, act: lte-m, tac: 3866, earfcn: 6200, pci: 14, rsrp: -75 dBm\n"
		"  cell: 074FEBAC, act: nb-iot, tac: 28B4, earfcn: 6247, pci: 14, rsrp: -68 dBm\n"
		"plmn: 23002, act: lte-m,nb-iot, stat: current (lte-m), available (nb-iot)\n"
		"  cell: 074FEB50, act: lte-m, tac: 05F2, earfcn: 6300, pci: 488, rsrp: -74 dBm\n"
		"  cell: 074FEB51, act: lte-m, tac: 05F2, earfcn: 6300, pci: 339, rsrp: -76 dBm\n"
		"plmn: 23003, act: lte-m,nb-iot, stat: available\n"
		"  cell: 000AE5CA, act: lte-m, tac: 8DCC, earfcn: 3544, pci: 135, rsrp: -66 dBm\n"
		"networks: 3 (6 with act), cells: 5\n",
		"%s", out);
	k_sleep(K_SECONDS(65));
	out = shell_out("lte scan show");
	zassert_true(!strncmp(out, "last scan: 1m 05s ago (request)\n", 32), "%s", out);

	assert_restored();
	zassert_equal(atomic_get(&m_scan_cb_count), 1);
	/* And the restore by one more */
	zassert_equal(mock_ctr_lte_link_reset_count() - resets, 3);
	/* Each right before a prepare: the first RAT before any command, the switch and the restore
	 * after the CFUN=0 that follows the answered +COPS=? */
	assert_reset_at(0, TX_INDEX(items, "AT", 1));
	assert_reset_at(1, TX_INDEX(items, "AT+CFUN=0", 2) + 1);
	assert_reset_at(2, TX_INDEX(items, "AT+CFUN=0", 4) + 1);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_ONE_RAT)
/* LTE-M only; the scan cancels the measurement that waits for its URC, a transaction that waits
 * for the measurement goes first */
ZTEST(scan, test_scan_one_rat)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	const char *out = shell_out("lte scan show");
	zassert_str_equal(out, "no scan yet\n", "%s", out);

	/* clang-format off */
	static struct mock_link_item items[] = {
		/* The measurement from the ready state as the board with PSM logged it, no URC */
		URC(1000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(0, "%XMODEMSLEEP: 1,86399601"),
		URC(0, "%XMODEMSLEEP: 1,0"),
		{"AT%NCELLMEASSTOP", "OK"},
		/* Registered: the transaction before the scan */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		URC(500, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_SCAN_CELLS_COPS_LTEM,
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	waiter_start(ctr_lte_v2_wait_for_ncellmeas);
	k_sleep(K_SECONDS(5));
	ASSERT_FSM_STATE("ncellmeas");

	/* The measurement state leaves the transaction waiting */
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	k_sleep(K_MSEC(100));
	ASSERT_FSM_STATE("ncellmeas");

	/* Returns at once */
	int64_t start = k_uptime_get();
	out = shell_out("lte scan schedule");
	zassert_str_equal(out, "command succeeded\n", "%s", out);
	zassert_true(k_uptime_get() - start < 1000);

	zassert_ok(waiter_join());
	assert_result(-ECANCELED, 0, false);
	zassert_equal(atomic_get(&m_cb_count), 1);

	int64_t elapsed;
	zassert_ok(stack_caller_join(&elapsed));
	zassert_equal(m_recv_len, 8);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(0, 0, 0, -ENODATA, -ENODATA, -ENODATA, 4, 3);
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	assert_restored();
	zassert_equal(atomic_get(&m_cb_count), 1);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_TIMEOUTS)
/* LTE-M: no RRC idle, no URC; the cells found until the stop, a list longer than kept. The last
 * measurement is not shown while the step measures */
ZTEST(scan, test_scan_timeouts)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items_meas[] = {
		ITEMS_NCELLMEAS(1000, 1000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
	};

	static struct mock_link_item items[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		/* SLM announcing itself after the boot of the step (not seen on the modem): no
		 * prepare */
		URC(1000, "Ready"),
		ITEMS_SCAN_REGISTER_LTEM,
		{"AT%NCELLMEAS=5,7", "OK"},
		{"AT%NCELLMEASSTOP", "OK"},
		URC(10, "%NCELLMEAS: " HW_NCELLMEAS_STOPPED),
		ITEMS_SCAN_COPS(5000, SYN_COPS_LONG),
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_meas, ARRAY_SIZE(items_meas));
	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(5)));
	assert_result(0, 4, true);
	stack_wait_for_state("ready", K_SECONDS(1));
	mock_ctr_lte_link_assert_done();

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));

	/* In the cells step: the measurement of the step runs */
	for (int i = 0; i < 300 && TX_TIME(items, "AT%NCELLMEAS=5,7", 1) < 0; i++) {
		k_sleep(K_SECONDS(1));
	}
	k_sleep(K_SECONDS(5));
	assert_result(-ENODATA, 0, false);
	const char *out = shell_out("lte ncellmeas show");
	zassert_str_equal(out, "no measurement yet\ncommand succeeded\n", "%s", out);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));

	assert_tx_time(TX_INDEX(items, "AT%NCELLMEAS=5,7", 1),
		       URC_TIME(items, CEREG_NO_PSM, 1) + T_IDLE_MS);
	assert_tx_time(TX_INDEX(items, "AT%NCELLMEASSTOP", 1),
		       TX_TIME(items, "AT%NCELLMEAS=5,7", 1) + T_MEASURE_MS);
	assert_tx_time(TX_INDEX(items, "AT+COPS=?", 1),
		       URC_TIME(items, "%NCELLMEAS: " HW_NCELLMEAS_STOPPED, 1));

	assert_scan(0, -ETIMEDOUT, 0, -ENODATA, -ENODATA, -ENODATA, 3, 12);
	for (int i = 0; i < 3; i++) {
		zassert_equal(m_scan.cells[i].act, 7);
	}
	zassert_str_equal(m_scan.networks[9].plmn, "26210");

	/* The cells until the stop, the networks stored */
	out = no_ages(shell_out("lte scan show"));
	zassert_str_equal(
		out,
		"last scan: * ago (request)\n"
		"search: lte-m cells partial, nb-iot cells not run, networks ok\n"
		"plmn: 23002, act: lte-m, stat: not in network list\n"
		"  cell: 074FEB50, act: lte-m, tac: 05F2, earfcn: 6300, pci: 488, rsrp: -74 dBm\n"
		"  cell: 074FEB51, act: lte-m, tac: 05F2, earfcn: 6300, pci: 339, rsrp: -75 dBm\n"
		"plmn: 23003, act: lte-m, stat: not in network list\n"
		"  cell: 000AE5CA, act: lte-m, tac: 8DCC, earfcn: 3544, pci: 135, rsrp: -66 dBm\n"
		"plmn: 26201, act: lte-m, stat: available\n"
		"plmn: 26202, act: lte-m, stat: available\n"
		"plmn: 26203, act: lte-m, stat: available\n"
		"plmn: 26204, act: lte-m, stat: available\n"
		"plmn: 26205, act: lte-m, stat: available\n"
		"plmn: 26206, act: lte-m, stat: available\n"
		"plmn: 26207, act: lte-m, stat: available\n"
		"plmn: 26208, act: lte-m, stat: available\n"
		"plmn: 26209, act: lte-m, stat: available\n"
		"plmn: 26210, act: lte-m, stat: available\n"
		"networks: 10 (12 with act, 10 stored), cells: 3\n",
		"%s", out);

	/* The step also leaves its cells to 'lte ncellmeas show' */
	assert_result(-ETIMEDOUT, 3, true);

	assert_restored();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_COPS_TIMEOUT)
/* AT+COPS=? gets no answer: no NB-IoT, the reset of the restore gets the modem back */
ZTEST(scan, test_scan_cops_timeout)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_NCELLMEAS(10000, 50000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		{"AT+COPS=?"},
		/* No CFUN=0: the modem may still search */
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));

	assert_scan(0, 0, -ETIMEDOUT, -ENODATA, -ENODATA, -ENODATA, 4, 0);

	assert_restored();
	/* The reset of the restore right after the dialog */
	assert_tx_time(TX_INDEX(items, "AT", 2), TX_TIME(items, "AT+COPS=?", 1) + T_COPS_MS);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_COPS_CME)
/* The modem rejects the measurement and the list (+CME ERROR, ERROR): each step ends at once. The
 * RRC connection of LTE-M is not released, the reset for NB-IoT drops it. CFUN=0 before the resets
 * gets no answer, then an ERROR: the resets follow anyway */
ZTEST(scan, test_scan_cops_cme)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		{"AT%NCELLMEAS=5,7", "+CME ERROR: 0"},
		{"AT+COPS=?", "+CME ERROR: 0"},
		{"AT+CFUN=0"},
		ITEMS_SCAN_NBIOT_CAMPED,
		{"AT+COPS=?", "ERROR"},
		{"AT+CFUN=0", "ERROR"},
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	int resets = mock_ctr_lte_link_reset_count();

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));

	assert_tx_time(TX_INDEX(items, "AT%NCELLMEAS=5,7", 1),
		       URC_TIME(items, CEREG_NO_PSM, 1) + T_IDLE_MS);
	assert_tx_time(TX_INDEX(items, "AT+COPS=?", 1), TX_TIME(items, "AT%NCELLMEAS=5,7", 1));
	/* CFUN=0 at once, NB-IoT at the end of its 120 s */
	assert_tx_time(TX_INDEX(items, "AT+CFUN=0", 2), TX_TIME(items, "AT+COPS=?", 1));
	assert_tx_time(TX_INDEX(items, "AT", 2), TX_TIME(items, "AT+CFUN=0", 2) + T_CFUN0_MS);
	/* The reset dropped the RRC connection of LTE-M: no wait for the idle */
	assert_tx_time(TX_INDEX(items, "AT%NCELLMEAS=5,7", 2),
		       TX_TIME(items, "AT+CFUN=1", 2) + T_REGISTER_MS);
	assert_scan(0, -EIO, -EIO, -ETIMEDOUT, 0, -EIO, 1, 0);
	/* Both lists rejected */
	const char *out = shell_out("lte scan show");
	zassert_not_null(
		strstr(out, "\nsearch: lte-m cells error, nb-iot cells ok, networks error\n"), "%s",
		out);

	assert_restored();
	zassert_equal(mock_ctr_lte_link_reset_count() - resets, 3);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_STEP_FAIL)
/* The start of the RAT fails: the scan ends, the restore follows */
ZTEST(scan, test_scan_step_fail)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* 1. The modem rejects the CFUN=1 of the step */

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_PREPARE_SYSMODE("1,0,1,0"),
		{"AT+CFUN=1", "ERROR"},
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	assert_scan(-EIO, -ENODATA, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);

	assert_restored();
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	/* 2. No Ready after the reset of the step: the restore resets the modem again */

	/* clang-format off */
	static struct mock_link_item items_boot[] = {
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_boot, ARRAY_SIZE(items_boot));
	mock_ctr_lte_link_drop_ready(1);
	int resets = mock_ctr_lte_link_reset_count();
	int64_t start = k_uptime_get();

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	assert_scan(-ETIMEDOUT, -ENODATA, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	zassert_equal(atomic_get(&m_scan_cb_count), 2);

	assert_restored();
	zassert_equal(mock_ctr_lte_link_reset_count() - resets, 2);
	/* At the end of the wait for Ready */
	assert_tx_time(TX_INDEX(items_boot, "AT", 1), start + 10000);

	/* 3. 'lte scan now' while the restore waits for Ready: a reconnect drops the request, no
	 * result of it */

	/* clang-format off */
	static struct mock_link_item items_drop[] = {
		{"AT#XSLEEP=2", "OK"},
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_drop, ARRAY_SIZE(items_drop));
	mock_ctr_lte_link_drop_ready(2);

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));

	int ret;

	shell_bg_start("lte scan now");
	k_sleep(K_SECONDS(2));
	zassert_ok(ctr_lte_v2_reconnect());
	const char *out = shell_bg_join(&ret, K_SECONDS(30));

	zassert_equal(ret, -ECANCELED, "%s", out);
	zassert_str_equal(out, "scanning, this takes minutes...\nscan dropped\n", "%s", out);
	assert_restored();
	zassert_equal(atomic_get(&m_scan_cb_count), 3);

	/* 4. No Ready after the restore either: the error state refuses, no request */

	/* clang-format off */
	static struct mock_link_item items_error[] = {
		{"AT+CFUN?", "ERROR"},
		{"AT#XSLEEP=2", "OK"},
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_error, ARRAY_SIZE(items_error));
	mock_ctr_lte_link_drop_ready(2);

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	stack_wait_for_state("error", K_SECONDS(30));

	assert_shell_fails("lte scan now", -EBUSY,
			   "scan not possible: LTE error, retrying (fsm-state: error)");
	zassert_ok(ctr_lte_v2_wait_for_scan(K_NO_WAIT));
	assert_restored();
	zassert_equal(atomic_get(&m_scan_cb_count), 4);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_SEND)
/* NB-IoT with PSM: a transaction and the scan wake the modem up together, the transaction goes
 * first; one during the scan fails at once. The modem, in PSM again, answers the CFUN=0 before the
 * reset of the restore after 85 s: the reset waits for it. A measurement requested with the scan
 * keeps no sleep of before the scan */
ZTEST(scan, test_scan_send)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		URC(400, "+CSCON: 1"),
		{"AT#XRECV=5"},
		RX_DATA(200, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		ITEMS_SCAN_REGISTER_PSM,
		ITEMS_SCAN_CELLS_COPS_PSM,
		/* As the board with PSM answered it (NB-IoT, mfw 1.3.6) */
		{"AT+CFUN=0"},
		URC(85000, "OK"),
		ITEMS_SCAN_RESTORE_RESET,
		ITEMS_TXN,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	int64_t elapsed;

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	zassert_ok(stack_caller_join(&elapsed));
	zassert_equal(m_recv_len, 8);

	/* Offline: also a caller without a deadline gets no AT#XSEND */
	stack_wait_for_state("scan", K_SECONDS(2));
	stack_caller_start(&m_txn_forever, K_PRIO_PREEMPT(5));
	zassert_equal(stack_caller_join(&elapsed), -ENOTCONN);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(-ENODATA, -ENODATA, -ENODATA, 0, 0, 0, 3, 3);

	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(150)));
	/* The reset of the restore right after the answer */
	assert_reset_at(1, TX_INDEX(items, "AT+CFUN=0", 2) + 2);
	assert_tx_time(TX_INDEX(items, "AT", 2), TX_TIME(items, "AT+CFUN=0", 2) + 85000);
	txn_ok();
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	/* 2. Asleep in PSM, a measurement and the scan: the scan goes first. After it the network
	 * grants no PSM and the measurement ends without a URC: the ready state switches the radio
	 * off, there is no sleep left from before the scan */

	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items_meas[] = {
		/* The modem rejects the prepare of the scan */
		{"AT", "ERROR"},
		ITEMS_PREPARE,
		ITEMS_REGISTER_NO_PSM,
		URC(1000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "+CME ERROR: 0"},
		ITEMS_CFUN4_SLEEP,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_meas, ARRAY_SIZE(items_meas));

	zassert_ok(ctr_lte_v2_schedule_ncellmeas());
	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	assert_scan(-ENODATA, -ENODATA, -ENODATA, -EIO, -ENODATA, -ENODATA, 0, 0);
	zassert_ok(ctr_lte_v2_wait_for_ncellmeas(K_SECONDS(60)));
	assert_result(-EIO, 0, false);

	stack_wait_for_state("sleep", K_SECONDS(5));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_SEND_CFUN4)
/* Without PSM the radio is off (CFUN=4) in the ready state before the sleep and in the sleep
 * state: the scan goes before the transaction, which after the restore needs the radio on again */
ZTEST(scan, test_scan_send_cfun4)
{
	stack_attach();
	stack_go_to_sleep();

	/* clang-format off */
	static struct mock_link_item items_off[] = {
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
		{"AT#XSOCKET=0", "#XSOCKET: 0,\"closed\"", "OK"},
		/* The OK late: the transaction and the request come meanwhile */
		{"AT+CFUN=4"},
		URC(1000, "+CEREG: 0"),
		URC(0, "OK"),
		URC(0, "%XMODEMSLEEP: 4"),
		/* Then the scan, not the sleep; the modem rejects its prepare */
		{"AT", "ERROR"},
		ITEMS_PREPARE,
		ITEMS_REGISTER_NO_PSM,
		ITEMS_CFUN4_SLEEP,
	};

	static struct mock_link_item items[] = {
		/* No CFUN=1 of the wake-up, no AT#XSEND */
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_SCAN_CELLS_COPS_LTEM,
		ITEM_SCAN_CFUN0,
		ITEMS_PREPARE,
		ITEMS_REGISTER_NO_PSM,
		/* Before the end of the 500 ms of the ready state without PSM */
		ITEMS_FLOW_CHECK,
		{"AT#XSOCKETOPT=1,52", "OK"},
		{"AT#XSEND=\"Hello\"", "#XSEND: 5", "OK"},
		{"AT#XRECV=5"},
		/* The check before the send asked for the CEREG of the PSM cell */
		URC(100, CEREG_NO_PSM),
		RX_DATA(100, "#XRECV: 8", "Hi there"),
		{"AT#XSOCKETOPT=1,50", "OK"},
		ITEM_CONEVAL,
		ITEMS_CFUN4_SLEEP,
	};
	/* clang-format on */

	int64_t elapsed;

	/* 1. In the ready state with the radio off, before the sleep */

	mock_ctr_lte_link_start(items_off, ARRAY_SIZE(items_off));
	txn_ok();

	for (int i = 0; i < 500 && TX_TIME(items_off, "AT+CFUN=4", 1) < 0; i++) {
		k_sleep(K_MSEC(10));
	}
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	k_sleep(K_MSEC(10));
	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_equal(stack_caller_join(&elapsed), -ENOTCONN);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	assert_scan(-EIO, -ENODATA, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	stack_wait_for_state("sleep", K_SECONDS(30));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	/* 2. In the sleep state */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	/* The request alone wakes the modem up, the transaction comes during the wake-up */
	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	k_sleep(K_MSEC(50));
	ASSERT_FSM_STATE("wakeup");
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	zassert_equal(stack_caller_join(&elapsed), -ENOTCONN);

	/* The state follows the modem in the scan, not the registration kept while the radio was
	 * off: searching after the CFUN=1 of the step */
	for (int i = 0; i < 100 && TX_TIME(items, "AT+CFUN=1", 1) < 0; i++) {
		k_sleep(K_MSEC(100));
	}
	stack_sleep_until(TX_TIME(items, "AT+CFUN=1", 1) + 1500);
	struct ctr_lte_v2_cereg_param cereg;
	zassert_ok(ctr_lte_v2_get_cereg_param(&cereg));
	zassert_equal(cereg.stat, CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING, "stat %d", cereg.stat);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(0, 0, 0, -ENODATA, -ENODATA, -ENODATA, 4, 3);

	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(60)));
	txn_ok();
	stack_wait_for_state("sleep", K_SECONDS(5));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_SEND_AFTER)
/* Registered: a transaction queued after the request goes first; one after the start of the scan
 * fails at once */
ZTEST(scan, test_scan_send_after)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_TXN,
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_SCAN_CELLS_COPS_LTEM,
		ITEMS_SCAN_RESTORE,
		ITEMS_TXN,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	int64_t elapsed;

	/* The caller runs before the FSM: the ready state sees both */
	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	zassert_ok(stack_caller_join(&elapsed));

	stack_wait_for_state("scan", K_SECONDS(2));
	k_sleep(K_MSEC(10));
	stack_caller_start(&m_txn, K_PRIO_PREEMPT(5));
	zassert_equal(stack_caller_join(&elapsed), -ENOTCONN);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(0, 0, 0, -ENODATA, -ENODATA, -ENODATA, 4, 3);

	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(60)));
	txn_ok();
	k_sleep(K_MSEC(500));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_RECONNECT)
/* LTE reconnects during the registration, during AT+COPS=? of LTE-M, then during its cells */
ZTEST(scan, test_scan_reconnect)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* 1. The step running is cancelled, the boot after the disabled state restores */

	/* clang-format off */
	static struct mock_link_item items_reg[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_reg, ARRAY_SIZE(items_reg));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	k_sleep(K_SECONDS(5));
	ASSERT_FSM_STATE("scan");
	stack_shell_cmd("lte reconnect");

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(10)));
	assert_scan(-ECANCELED, -ENODATA, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	zassert_equal(atomic_get(&m_scan_cb_count), 1);
	assert_restored();

	/* 2. Handled right after the dialog, before NB-IoT starts */

	/* clang-format off */
	static struct mock_link_item items_cops[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_SCAN_CELLS_COPS_LTEM,
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cops, ARRAY_SIZE(items_cops));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));

	for (int i = 0; i < 120 && TX_TIME(items_cops, "AT+COPS=?", 1) < 0; i++) {
		k_sleep(K_SECONDS(1));
	}
	k_sleep(K_SECONDS(10));
	stack_shell_cmd("lte reconnect");

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(120)));
	assert_scan(0, 0, 0, -ENODATA, -ENODATA, -ENODATA, 4, 3);
	zassert_equal(atomic_get(&m_scan_cb_count), 2);
	assert_restored();

	/* 3. The measurement of the step is stopped */

	/* clang-format off */
	static struct mock_link_item items_cells[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		URC(10000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		{"AT%NCELLMEASSTOP", "OK"},
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cells, ARRAY_SIZE(items_cells));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));

	for (int i = 0; i < 120 && TX_TIME(items_cells, "AT%NCELLMEAS=5,7", 1) < 0; i++) {
		k_sleep(K_SECONDS(1));
	}
	k_sleep(K_SECONDS(5));
	stack_shell_cmd("lte reconnect");

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(10)));
	assert_scan(0, -ECANCELED, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	zassert_equal(atomic_get(&m_scan_cb_count), 3);
	assert_restored();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_IN_ATTACH)
/* Requested during the attach, which the scan cuts short; an unreadable list still waits for OK */
ZTEST(scan, test_scan_in_attach)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_PREPARE,
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_NCELLMEAS(10000, 50000, "%NCELLMEAS: " HW_NCELLMEAS_REGISTERED),
		{"AT+COPS=?"},
		URC(5000, "+COPS: " SYN_COPS_BAD),
		/* A command before the OK is a violation */
		URC(100, "OK"),
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_enable());
	stack_wait_for_state("attach", K_SECONDS(10));

	struct ctr_lte_v2_metrics metrics;
	int attempt;

	zassert_ok(ctr_lte_v2_get_metrics(&metrics));
	zassert_equal(metrics.attach_count, 1);

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));

	assert_scan(0, 0, -EBADMSG, -ENODATA, -ENODATA, -ENODATA, 4, 0);

	assert_restored();
	/* The restore right after the OK, not at the end of the dialog bound */
	assert_tx_time(TX_INDEX(items, "AT", 3), URC_TIME(items, "OK", 1));

	/* The cut attach and the new one count, no failure, the same attempt */
	zassert_ok(ctr_lte_v2_get_metrics(&metrics));
	zassert_equal(metrics.attach_count, 2);
	zassert_equal(metrics.attach_fail_count, 0);
	zassert_ok(ctr_lte_v2_get_curr_attach_info(&attempt, NULL, NULL, NULL));
	zassert_equal(attempt, 1);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_IN_RETRY_DELAY)
/* The device that does not attach (NB-IoT, rejected as on the board with PSM): the scan cuts the
 * retry delay short and runs after the attach starts */
ZTEST(scan, test_scan_in_retry_delay)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		/* Three attaches fail, only the third has a retry delay (periodic-2h) */
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		ITEMS_PREPARE,
		ITEMS_RETRY_DELAY,
		ITEMS_PREPARE,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		/* The reject (gate 3B) is stat 2 with a cause: the step waits for its bound */
		URC(1000, HW_REJECT_CEREG_SEARCH),
		URC(1500, "+CSCON: 1"),
		URC(2000, "+CNEC_EMM: 11"),
		URC(100, HW_REJECT_CEREG),
		URC(100, "%MDMEV: SEARCH STATUS 2"),
		URC(1900, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(126000, "%NCELLMEAS: " HW_REJECT_NCELLMEAS),
		ITEMS_SCAN_COPS(92000, HW_REJECT_COPS),
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	g_ctr_lte_v2_config.attach_policy = CTR_LTE_V2_ATTACH_POLICY_PERIODIC_2H;
	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_enable());
	/* Past the attaches of 5, 5 and 50 min (each retry delay state runs a while) */
	k_sleep(K_MINUTES(62));
	ASSERT_FSM_STATE("retry_delay");

	int attempt;
	int attach_timeout_sec;

	zassert_ok(ctr_lte_v2_get_curr_attach_info(&attempt, NULL, NULL, NULL));
	zassert_equal(attempt, 3);

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	stack_wait_for_state("scan", K_SECONDS(30));

	/* The next step of the policy */
	zassert_ok(ctr_lte_v2_get_curr_attach_info(&attempt, &attach_timeout_sec, NULL, NULL));
	zassert_equal(attempt, 4);
	zassert_equal(attach_timeout_sec, 300);

	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_tx_time(TX_INDEX(items, "AT%NCELLMEAS=5,7", 1),
		       TX_TIME(items, "AT+CFUN=1", 5) + T_REGISTER_MS);
	assert_scan(-ENODATA, -ENODATA, -ENODATA, -ETIMEDOUT, 0, 0, 3, 3);
	zassert_equal(m_scan.cells[0].eci, 0x074FEB56);
	zassert_true(m_scan.cells[0].serving);
	zassert_str_equal(m_scan.networks[0].plmn, "23002");
	zassert_equal(m_scan.networks[0].stat, 3);

	/* The reject first, as 'lte history' shows it */
	const char *out = no_ages(shell_out("lte scan show"));
	zassert_str_equal(
		out,
		"last-reject: * ago, searching, act: nb-iot, tac: 2D02, cell: 074FEB56, emm: 11 "
		"(PLMN not allowed)\n"
		"last-reject-hint: network not allowed for this SIM, check roaming\n"
		"last scan: * ago (request)\n"
		"search: lte-m cells not run, nb-iot cells ok, networks ok\n"
		"plmn: 23001, act: nb-iot, stat: available\n"
		"  cell: 074FEBAC, act: nb-iot, tac: 28B4, earfcn: 6247, pci: 14, rsrp: -64 dBm\n"
		"plmn: 23002, act: nb-iot, stat: forbidden\n"
		"  cell: 074FEB56, act: nb-iot, tac: 2D02, earfcn: 6348, pci: 339, rsrp: -70 dBm\n"
		"plmn: 23003, act: nb-iot, stat: available\n"
		"  cell: 000AE520, act: nb-iot, tac: B4DC, earfcn: 6447, pci: 135, rsrp: -56 dBm\n"
		"networks: 3 (3 with act), cells: 3\n",
		"%s", out);

	assert_restored();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_IN_GNSS)
/* GNSS with PSM: the scan goes through the ready state, the GNSS runs again after the restore */
ZTEST(scan, test_scan_in_gnss)
{
	stack_attach();

	/* clang-format off */
	static struct mock_link_item items_gnss[] = {
		{"AT#XGPS=1,0,1"},
	};

	static struct mock_link_item items[] = {
		{"AT#XGPS=0"},
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		ITEMS_SCAN_REGISTER_PSM,
		ITEMS_SCAN_CELLS_COPS_PSM,
		ITEMS_SCAN_RESTORE,
		{"AT#XGPS=1,0,1"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_gnss, ARRAY_SIZE(items_gnss));
	zassert_ok(ctr_lte_v2_gnss_set_enable(true));
	stack_wait_for_state("gnss", K_SECONDS(2));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(-ENODATA, -ENODATA, -ENODATA, 0, 0, 0, 3, 3);

	stack_wait_for_state("gnss", K_SECONDS(60));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_IN_GNSS_NO_PSM)
/* GNSS without PSM (LTE off by CFUN=20): the scan goes through the attach state */
ZTEST(scan, test_scan_in_gnss_no_psm)
{
	/* clang-format off */
	static struct mock_link_item items_attach[] = {
		ITEMS_PREPARE,
		ITEMS_REGISTER_NO_PSM,
		{"AT+CFUN=20", "OK"},
		{"AT#XGPS=1,0,1"},
	};

	static struct mock_link_item items[] = {
		{"AT#XGPS=0"},
		{"AT+CFUN=21", "OK"},
		/* At once, not after a registration */
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		ITEMS_SCAN_CELLS_COPS_LTEM,
		ITEM_SCAN_CFUN0,
		ITEMS_PREPARE,
		ITEMS_REGISTER_NO_PSM,
		{"AT+CFUN=20", "OK"},
		{"AT#XGPS=1,0,1"},
	};
	/* clang-format on */

	zassert_ok(ctr_lte_v2_gnss_set_enable(true));
	mock_ctr_lte_link_start(items_attach, ARRAY_SIZE(items_attach));
	zassert_ok(ctr_lte_v2_enable());
	stack_wait_for_state("gnss", K_SECONDS(30));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));
	assert_scan(0, 0, 0, -ENODATA, -ENODATA, -ENODATA, 4, 3);

	stack_wait_for_state("gnss", K_SECONDS(60));
	k_sleep(K_MSEC(500));
	mock_ctr_lte_link_assert_done();
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_RESET_LOOP)
/* The reset loop ends the scan, also in the cells step; a request deferred in the reset loop is
 * dropped by a reconnect. 'lte scan now' refuses the reset loop, also one that comes while it
 * waits */
ZTEST(scan, test_scan_reset_loop)
{
	zassert_ok(ctr_lte_v2_add_callback(&m_cb));
	stack_attach();

	/* 1. The restore prepares the configured system mode (and clears the count of the modem by
	 * its CFUN=0); the reset loop again in the attach goes to the reset loop state */

	/* clang-format off */
	static struct mock_link_item items_loop[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		URC(3000, "%MDMEV: RESET LOOP"),
		ITEMS_PREPARE,
		URC(1000, "%MDMEV: RESET LOOP"),
		{"AT+CFUN=4", "OK"},
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_loop, ARRAY_SIZE(items_loop));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(30)));
	assert_scan(-ECANCELED, -ENODATA, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	stack_wait_for_state("reset_loop", K_SECONDS(10));
	k_sleep(K_SECONDS(6));
	mock_ctr_lte_link_assert_done();

	/* No request */
	assert_shell_fails(
		"lte scan now", -EBUSY,
		"scan not possible: modem reset loop, attach blocked (fsm-state: reset_loop)");
	zassert_ok(ctr_lte_v2_wait_for_scan(K_NO_WAIT));

	/* 2. Deferred, then dropped: the waiting caller returns, no callback, no scan */

	/* clang-format off */
	static struct mock_link_item items_drop[] = {
		/* The reset loop restarts the modem on leaving */
		{"AT+CFUN=0", "OK"},
		{"AT+CFUN=1", "OK"},
		{"AT#XSLEEP=2", "OK"},
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_drop, ARRAY_SIZE(items_drop));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	waiter_start(ctr_lte_v2_wait_for_scan);
	k_sleep(K_MSEC(100));
	ASSERT_FSM_STATE("reset_loop");

	/* Already requested is fine; the previous result meanwhile */
	const char *out = shell_out("lte scan schedule");
	zassert_str_equal(out, "command succeeded\n", "%s", out);
	out = no_ages(shell_out("lte scan show"));
	zassert_str_equal(out,
			  "last scan: * ago (request)\n"
			  "search: lte-m cells not run, nb-iot cells not run, networks not run\n"
			  "networks: 0 (0 with act), cells: 0\n",
			  "%s", out);

	stack_shell_cmd("lte reconnect");
	zassert_ok(waiter_join());

	zassert_ok(ctr_lte_v2_wait_for_connected(K_SECONDS(60)));
	k_sleep(K_SECONDS(2));
	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_scan_cb_count), 1);

	/* 3. In the cells step: the reset of the restore ends the measurement, no AT%NCELLMEASSTOP
	 * before it */

	/* clang-format off */
	static struct mock_link_item items_cells[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		URC(10000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		URC(5000, "%MDMEV: RESET LOOP"),
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cells, ARRAY_SIZE(items_cells));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(60)));
	assert_scan(0, -ECANCELED, -ENODATA, -ENODATA, -ENODATA, -ENODATA, 0, 0);
	zassert_equal(atomic_get(&m_scan_cb_count), 2);
	out = shell_out("lte scan show");
	zassert_not_null(
		strstr(out,
		       "\nsearch: lte-m cells cancelled, nb-iot cells not run, networks not run\n"),
		"%s", out);

	assert_restored();
	assert_reset_at(0, TX_INDEX(items_cells, "AT", 1));
	assert_reset_at(1, TX_INDEX(items_cells, "AT", 2));
	zassert_equal(mock_ctr_lte_link_reset_item(2), -1);

	/* 4. 'lte scan now' in the restore: a reset loop while it waits, the request stays */

	/* clang-format off */
	static struct mock_link_item items_wait[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		URC(3000, "%MDMEV: RESET LOOP"),
		/* The restore: the reset loop in the prepare, before the SIM (and the attach that
		 * would take the request) */
		ITEMS_PREPARE_SYSMODE(SYSMODE),
		{"AT+CFUN=1", "OK"},
		URC(500, "%MDMEV: RESET LOOP"),
		{"AT+CFUN=4", "OK"},
		{"AT#XSLEEP=2", "OK"},
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_wait, ARRAY_SIZE(items_wait));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(30)));

	int ret;

	shell_bg_start("lte scan now");
	out = shell_bg_join(&ret, K_SECONDS(30));
	zassert_equal(ret, -EBUSY, "%s", out);
	zassert_str_equal(
		out,
		"scanning, this takes minutes...\n"
		"scan not possible: modem reset loop, attach blocked (fsm-state: reset_loop)\n",
		"%s", out);
	ASSERT_FSM_STATE("reset_loop");
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_ALL), -EALREADY);

	k_sleep(K_SECONDS(6));
	mock_ctr_lte_link_assert_done();
	zassert_equal(atomic_get(&m_scan_cb_count), 3);
}
#endif

#if defined(CONFIG_TEST_FEATURE_SCAN_SCAN_MODES)
/* The result of 'lte scan now cells' below */
#define SHOW_SCAN_CELLS                                                                            \
	"last scan: * ago (request)\n"                                                             \
	"search: lte-m cells partial, nb-iot cells partial, networks not run\n"                    \
	"plmn: 23002, act: lte-m, stat: not in network list\n"                                     \
	"  cell: 074FEB50, act: lte-m, tac: 05F2, earfcn: 6300, pci: 488, rsrp: -74 dBm\n"         \
	"  cell: 074FEB51, act: lte-m, tac: 05F2, earfcn: 6300, pci: 339, rsrp: -75 dBm\n"         \
	"plmn: 23003, act: lte-m, stat: not in network list\n"                                     \
	"  cell: 000AE5CA, act: lte-m, tac: 8DCC, earfcn: 3544, pci: 135, rsrp: -66 dBm\n"         \
	"networks: 0 (0 with act), cells: 3\n"

/* Both RATs: the shell without a mode, the bad words, test mode; the cells only ('lte scan now
 * cells', it waits), then the networks only ('lte scan schedule plmn', at once) joined by 'lte scan
 * now'. A mode skips its step in each RAT; CFUN=0 before a reset only after an answer */
ZTEST(scan, test_scan_modes)
{
	/* No request */
	assert_shell_fails("lte scan now", -EBUSY,
			   "scan not possible: LTE is disabled (fsm-state: disabled)");
	assert_shell_fails("lte scan schedule", -ENODEV, "command failed");
	g_ctr_lte_v2_config.test = true;
	assert_shell_fails("lte scan now", -ENOEXEC, "not supported in test mode");
	assert_shell_fails("lte scan schedule cells", -ENOEXEC, "not supported in test mode");
	g_ctr_lte_v2_config.test = false;
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_CELLS + 1), -EINVAL);

	stack_attach();

	/* The help alone, no scan */
	const char *out = shell_out("lte scan");
	zassert_true(strstr(out, "Subcommands:") && strstr(out, "now") && strstr(out, "schedule") &&
			     strstr(out, "show"),
		     "%s", out);
	assert_shell_fails("lte scan foo", -EINVAL, "command not found: foo");
	assert_shell_fails("lte scan now foo", -EINVAL, "unknown mode: foo");
	assert_shell_fails("lte scan schedule foo", -EINVAL, "unknown mode: foo");
	assert_shell_fails("lte scan now all cells", -EINVAL, "wrong parameter count");
	zassert_ok(ctr_lte_v2_wait_for_scan(K_NO_WAIT));
	ASSERT_FSM_STATE("ready");
	out = shell_out("lte scan show");
	zassert_str_equal(out, "no scan yet\n", "%s", out);

	/* 1. The cells only */

	/* clang-format off */
	static struct mock_link_item items_cells[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		URC(10000, "+CSCON: 0"),
		{"AT%NCELLMEAS=5,7", "OK"},
		/* The cells found until the stop: the modem answered, CFUN=0 before the reset; no
		 * AT+COPS=? */
		{"AT%NCELLMEASSTOP", "OK"},
		URC(10, "%NCELLMEAS: " HW_NCELLMEAS_STOPPED),
		ITEM_SCAN_CFUN0,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		URC(17000, HW_CEREG_NBIOT_CAMPED),
		{"AT%NCELLMEAS=5,7", "OK"},
		/* No URC, not even after the stop: the reset alone */
		{"AT%NCELLMEASSTOP", "OK"},
		ITEMS_SCAN_RESTORE_RESET,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_cells, ARRAY_SIZE(items_cells));

	int resets = mock_ctr_lte_link_reset_count();

	/* shell_info, as in hio */
	out = stack_shell_cmd("lte scan now cells");
	zassert_not_null(strstr(out, "\x1b[1;32mcommand succeeded"), "%s", out);
	out = no_ages(shell_clean(out));
	zassert_str_equal(out, scan_now_text("", 16, SHOW_SCAN_CELLS), "%s", out);

	assert_scan(0, -ETIMEDOUT, -ENODATA, -ETIMEDOUT, -ETIMEDOUT, -ENODATA, 3, 0);
	zassert_equal(m_scan.mode, CTR_LTE_V2_SCAN_CELLS);
	assert_restored();
	zassert_equal(mock_ctr_lte_link_reset_count() - resets, 3);
	assert_reset_at(1, TX_INDEX(items_cells, "AT+CFUN=0", 2) + 1);
	assert_reset_at(2, TX_INDEX(items_cells, "AT%NCELLMEASSTOP", 2) + 1);

	/* 2. The networks only, the list in the RRC idle */

	/* clang-format off */
	static struct mock_link_item items_plmn[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		/* A command before the +CSCON: 0 is a violation */
		URC(10000, "+CSCON: 0"),
		ITEMS_SCAN_COPS(74000, HW_COPS_LIST),
		ITEM_SCAN_CFUN0,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		URC(17000, HW_CEREG_NBIOT_CAMPED),
		/* At the end of the registration bound; rejected, but answered */
		{"AT+COPS=?", "ERROR"},
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_plmn, ARRAY_SIZE(items_plmn));

	/* Returns at once; requested again with another mode: the first one runs */
	int64_t start = k_uptime_get();
	out = stack_shell_cmd("lte scan schedule plmn");
	zassert_not_null(strstr(out, "\x1b[1;32mcommand succeeded"), "%s", out);
	zassert_str_equal(shell_clean(out), "command succeeded\n", "%s", out);
	zassert_true(k_uptime_get() - start < 1000);
	out = shell_out("lte scan schedule");
	zassert_str_equal(out, "command succeeded\n", "%s", out);
	zassert_equal(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_CELLS), -EALREADY);

	k_sleep(K_SECONDS(1));
	ASSERT_FSM_STATE("scan");

	/* The previous result while it runs */
	out = no_ages(shell_out("lte scan show"));
	zassert_str_equal(out, SHOW_SCAN_CELLS, "%s", out);

	out = no_ages(shell_out("lte scan now"));
	zassert_str_equal(out,
			  scan_now_text("scan already in progress, waiting for it\n", 6,
					"last scan: * ago (request)\n"
					"search: lte-m cells not run, nb-iot cells not run, "
					"networks partial\n"
					"plmn: 23001, act: lte-m, stat: available\n"
					"plmn: 23002, act: lte-m, stat: current\n"
					"plmn: 23003, act: lte-m, stat: available\n"
					"networks: 3 (3 with act), cells: 0\n"),
			  "%s", out);

	assert_tx_time(TX_INDEX(items_plmn, "AT+COPS=?", 1), URC_TIME(items_plmn, "+CSCON: 0", 1));
	assert_tx_time(TX_INDEX(items_plmn, "AT+COPS=?", 2),
		       TX_TIME(items_plmn, "AT+CFUN=1", 2) + T_REGISTER_MS);
	assert_scan(0, -ENODATA, 0, -ETIMEDOUT, -ENODATA, -EIO, 0, 3);
	zassert_equal(m_scan.mode, CTR_LTE_V2_SCAN_PLMN);
	assert_restored();

	/* 3. The networks only, the RRC connection stays: the list at the bound of the idle wait.
	 * A +CSCON: 0 with a +CSCON: 1 after it in one batch is stale. MNCs of 3 digits */

	/* clang-format off */
	static struct mock_link_item items_busy[] = {
		ITEMS_SCAN_RAT_START("1,0,1,0"),
		ITEMS_SCAN_REGISTER_LTEM,
		URC(10000, "+CSCON: 0"),
		URC(0, "+CSCON: 1"),
		ITEMS_SCAN_COPS(5000, "(1,\"\",\"\",\"311090\",7),(2,\"\",\"\",\"310410\",7),"
				      "(1,\"\",\"\",\"23003\",7)"),
		ITEM_SCAN_CFUN0,
		ITEMS_SCAN_RAT_START("0,1,1,0"),
		URC(17000, HW_CEREG_NBIOT_CAMPED),
		{"AT+COPS=?", "ERROR"},
		ITEMS_SCAN_RESTORE,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items_busy, ARRAY_SIZE(items_busy));

	zassert_ok(ctr_lte_v2_scan(CTR_LTE_V2_SCAN_PLMN));
	zassert_ok(ctr_lte_v2_wait_for_scan(K_SECONDS(600)));

	assert_tx_time(TX_INDEX(items_busy, "AT+COPS=?", 1),
		       URC_TIME(items_busy, CEREG_NO_PSM, 1) + T_IDLE_MS);
	assert_scan(0, -ENODATA, 0, -ETIMEDOUT, -ENODATA, -EIO, 0, 3);

	/* In the order of MCC and MNC */
	out = no_ages(shell_out("lte scan show"));
	zassert_str_equal(out,
			  "last scan: * ago (request)\n"
			  "search: lte-m cells not run, nb-iot cells not run, networks partial\n"
			  "plmn: 23003, act: lte-m, stat: available\n"
			  "plmn: 310410, act: lte-m, stat: current\n"
			  "plmn: 311090, act: lte-m, stat: available\n"
			  "networks: 3 (3 with act), cells: 0\n",
			  "%s", out);
	assert_restored();
}
#endif

ZTEST_SUITE(scan, NULL, NULL, NULL, NULL, NULL);
