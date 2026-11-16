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
#include <zephyr/ztest.h>

/* Standard includes */
#include <stdbool.h>
#include <string.h>

void stack_attach(void)
{
	/* clang-format off */
	static struct mock_link_item items[] = {
		ITEMS_PREPARE,
		ITEMS_REGISTER,
	};
	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

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
