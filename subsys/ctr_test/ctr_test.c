/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_test, CONFIG_CTR_TEST_LOG_LEVEL);

static atomic_t m_is_blocked;

static struct k_poll_signal m_start_sig;

static int cmd_test(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	unsigned int signaled;
	int result;
	k_poll_signal_check(&m_start_sig, &signaled, &result);

	if (signaled != 0) {
		shell_warn(shell, "test mode already active");
		return -ENOEXEC;
	}

	if (atomic_get(&m_is_blocked)) {
		shell_error(shell, "test mode activation blocked");
		return -EBUSY;
	}

	k_poll_signal_raise(&m_start_sig, 0);

	return 0;
}

SHELL_CMD_REGISTER(test, NULL, "Start test mode.", cmd_test);

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&m_start_sig);

	LOG_INF("Waiting for test mode entry");

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_start_sig),
	};
	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(5000));
	if (ret == -EAGAIN) {
		LOG_INF("Test mode entry timed out");
		atomic_set(&m_is_blocked, true);
		return 0;

	} else if (ret) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	LOG_INF("Test mode started");

	for (;;) {
		k_sleep(K_FOREVER);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_TEST_INIT_PRIORITY);
