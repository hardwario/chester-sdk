/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_tc.h>
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_tc_shell, CONFIG_CTR_TC_LOG_LEVEL);

static int cmd_tc_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum ctr_tc_channel channel;

	if (strcmp(argv[1], "a1") == 0) {
		channel = CTR_TC_CHANNEL_A1;
	} else if (strcmp(argv[1], "a2") == 0) {
		channel = CTR_TC_CHANNEL_A2;
	} else if (strcmp(argv[1], "b1") == 0) {
		channel = CTR_TC_CHANNEL_B1;
	} else if (strcmp(argv[1], "b2") == 0) {
		channel = CTR_TC_CHANNEL_B2;
	} else {
		shell_error(shell, "invalid channel: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (strcmp(argv[2], "k") != 0) {
		shell_error(shell, "invalid type: %s", argv[2]);
		shell_help(shell);
		return -EINVAL;
	}

	int repetitions = 1;

	if (argc == 4) {
		repetitions = strtoll(argv[3], NULL, 10);

		if (repetitions < 1 || repetitions > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}
	}

	for (int i = 0; i < repetitions; i++) {
		if (i) {
			k_sleep(K_SECONDS(1));
		}

		if (repetitions > 1) {
			shell_print(shell, "repetition: %d/%d", i + 1, repetitions);
		}

		float temperature;

		ret = ctr_tc_read(channel, CTR_TC_TYPE_K, &temperature);
		if (ret) {
			LOG_ERR("Call `ctr_tc_read` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell, "temperature: %.3f celsius", (double)temperature);
	}

	shell_print(shell, "command succeeded");

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
	sub_tc,

	SHELL_CMD_ARG(read, NULL,
	              "Read temperature w/ optional number of repetitions"
		      "(format: <a1|a2|b1|b2> <k> [<1-3600>]).",
	              cmd_tc_read, 3, 1),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tc, &sub_tc, "thermocouple commands.", print_help);

/* clang-format on */
