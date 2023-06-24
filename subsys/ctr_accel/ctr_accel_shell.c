/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_accel.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_accel_shell, CONFIG_CTR_ACCEL_LOG_LEVEL);

static int cmd_accel_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float accel_x;
	float accel_y;
	float accel_z;
	int orientation;
	ret = ctr_accel_read(&accel_x, &accel_y, &accel_z, &orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "accel-axis-x: %.3f m/s^2", accel_x);
	shell_print(shell, "accel-axis-y: %.3f m/s^2", accel_y);
	shell_print(shell, "accel-axis-z: %.3f m/s^2", accel_z);
	shell_print(shell, "orientation: %d", orientation);

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
	sub_accel,

	SHELL_CMD_ARG(read, NULL,
	              "Read sensor data.",
	              cmd_accel_read, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(accel, &sub_accel, "Accelerometer commands.", print_help);

/* clang-format on */
