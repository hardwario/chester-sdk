/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_hygro.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ctr_hygro_shell, CONFIG_CTR_HYGRO_LOG_LEVEL);

static int cmd_hygro_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int repetitions = 1;

	if (argc == 2) {
		repetitions = strtoll(argv[1], NULL, 10);

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
			shell_print(shell, "Repetition: %d/%d", i + 1, repetitions);
		}

		float temperature;
		float humidity;
		ret = ctr_hygro_read(&temperature, &humidity);
		if (ret) {
			LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell, "Temperature: %.2f C", temperature);
		shell_print(shell, "Humidity: %.1f %%", humidity);
	}

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
	sub_hygro,

	SHELL_CMD_ARG(read, NULL,
	              "Read hygrometer "
	              "w/ optional number of repetitions (format: [<1-3600>]).",
		      cmd_hygro_read, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(hygro, &sub_hygro, "Hygrometer commands.", print_help);

/* clang-format on */
