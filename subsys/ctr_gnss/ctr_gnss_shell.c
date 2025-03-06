/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_gnss.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_gnss_shell, CONFIG_CTR_GNSS_LOG_LEVEL);

static int cmd_start(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;
	ret = ctr_gnss_start(&corr_id);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_start` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_stop(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;
	ret = ctr_gnss_stop(false, &corr_id);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_stop` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_state(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	bool running;
	int ret = ctr_gnss_is_running(&running);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_is_running` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "running: %s", running ? "yes" : "no");

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gnss,

	SHELL_CMD_ARG(start, NULL,
	              "Start receiver.",
	              cmd_start, 1, 0),

	SHELL_CMD_ARG(stop, NULL,
	              "Stop receiver.",
	              cmd_stop, 1, 0),

	SHELL_CMD_ARG(state, NULL,
	              "Get receiver state.",
	              cmd_state, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gnss, &sub_gnss, "GNSS commands.", print_help);

/* clang-format on */
