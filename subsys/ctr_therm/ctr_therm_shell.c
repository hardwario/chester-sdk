/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_therm_shell, CONFIG_CTR_THERM_LOG_LEVEL);

static int cmd_therm_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float temperature;
	ret = ctr_therm_read(&temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "temperature: %.2f C", temperature);

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
	sub_therm,

	SHELL_CMD_ARG(read, NULL,
	              "Read sensor data.",
	              cmd_therm_read, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(therm, &sub_therm, "Thermometer commands.", print_help);

/* clang-format on */
