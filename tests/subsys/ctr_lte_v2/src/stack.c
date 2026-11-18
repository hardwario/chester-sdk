/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "stack.h"

#include "ctr_lte_v2_state.h"

#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* clang-format off */
static struct mock_link_item m_attach_items[] = {
	ITEMS_PREPARE,
	ITEMS_REGISTER,
};
/* clang-format on */

int stack_attach_index(const char *tx)
{
	for (size_t i = 0; i < ARRAY_SIZE(m_attach_items); i++) {
		if (m_attach_items[i].tx && !strcmp(m_attach_items[i].tx, tx)) {
			return i;
		}
	}

	return -ENOENT;
}

void stack_attach(void)
{
	mock_ctr_lte_link_start(m_attach_items, ARRAY_SIZE(m_attach_items));

	ctr_lte_v2_enable();

	for (int i = 0; i < 60 && strcmp(ctr_lte_v2_get_state(), "open_socket"); i++) {
		k_sleep(K_MSEC(100));
	}
	ASSERT_FSM_STATE("open_socket");

	/* Attached to the network, the socket is not connected yet */
	bool attached = false;
	zassert_ok(ctr_lte_v2_is_attached(&attached));
	zassert_true(attached, "expected attached while the socket connects");
	zassert_equal(ctr_lte_v2_wait_for_connected(K_NO_WAIT), -ETIMEDOUT,
		      "expected not connected while the socket connects");

	int ret = ctr_lte_v2_wait_for_connected(K_MSEC(6000));
	zassert_ok(ret, "ctr_lte_v2_wait_for_connected failed");

	k_sleep(K_MSEC(500));

	ASSERT_FSM_STATE("ready");
	mock_ctr_lte_link_assert_done();
}

void stack_wait_for_state(const char *state, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);

	while (strcmp(ctr_lte_v2_get_state(), state) && !sys_timepoint_expired(end)) {
		k_sleep(K_MSEC(10));
	}

	zassert_equal(strcmp(ctr_lte_v2_get_state(), state), 0, "expected state %s but got: %s",
		      state, ctr_lte_v2_get_state());
}

int stack_send_recv(struct ctr_lte_v2_send_recv_param *param, int64_t *elapsed)
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
	m_caller_ret = stack_send_recv(m_caller_param, &m_caller_elapsed);
}

/* Call send_recv from a thread of the priority instead of the (cooperative) test thread */
void stack_caller_start(struct ctr_lte_v2_send_recv_param *param, int prio)
{
	m_caller_param = param;

	k_thread_create(&m_caller_thread, m_caller_stack, K_THREAD_STACK_SIZEOF(m_caller_stack),
			caller_entry, NULL, NULL, NULL, prio, 0, K_NO_WAIT);
}

int stack_caller_join(int64_t *elapsed)
{
	zassert_ok(k_thread_join(&m_caller_thread, K_SECONDS(60)));

	*elapsed = m_caller_elapsed;

	return m_caller_ret;
}

void stack_sleep_until(int64_t uptime)
{
	int64_t now = k_uptime_get();

	if (uptime > now) {
		k_sleep(K_MSEC(uptime - now));
	}
}

/* The output of a shell command */
const char *stack_shell_cmd(const char *cmd)
{
	const struct shell *sh = shell_backend_dummy_get_ptr();
	size_t size;

	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
	shell_backend_dummy_clear_output(sh);
	int ret = shell_execute_cmd(sh, cmd);
	const char *out = shell_backend_dummy_get_output(sh, &size);
	zassert_ok(ret, "%s failed: %d: %s", cmd, ret, out);

	return out;
}

/* Give a transaction up while the FSM is in a state without IO: the call returns at the deadline,
 * and the state is left alone */
void stack_give_up(const char *state)
{
	struct ctr_lte_v2_send_recv_param param = {
		.send_buf = "Hello world",
		.send_len = 11,
		.send_as_string = true,
		.timeout = K_SECONDS(2),
		.rai = true,
	};

	int64_t elapsed;
	int ret = stack_send_recv(&param, &elapsed);
	zassert_equal(ret, -ETIMEDOUT, "expected -ETIMEDOUT but got: %d", ret);
	zassert_true(elapsed >= 2000 && elapsed <= 2100, "returned %lld ms after the call",
		     elapsed);

	zassert_equal(strcmp(ctr_lte_v2_get_state(), state), 0, "expected state %s but got: %s",
		      state, ctr_lte_v2_get_state());
}

void stack_go_to_sleep(void)
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
