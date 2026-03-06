/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_work.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdlib.h>

/* ### Preserved code "includes" (begin) */
#include "app_modbus.h"
#include "app_serial.h"
#include <string.h>
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_sample();

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_send();

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

/* ### Preserved code "functions 1" (begin) */
/* ^^^ Preserved code "functions 1" (end) */

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app,

			       SHELL_CMD_ARG(config, NULL, "Configurations commands.",
					     app_config_cmd_config, 1, 3),

/* ### Preserved code "subcmd" (begin) */
/* ^^^ Preserved code "subcmd" (end) */

			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);

SHELL_CMD_REGISTER(sample, NULL, "Trigger immediate sampling", cmd_sample);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately", cmd_send);

/* ### Preserved code "functions 2" (begin) */
/* Serial commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_serial,
	SHELL_CMD_ARG(send, NULL, "Send hex and wait for response <hex> [timeout_s]", cmd_serial_send, 2, 1),
	SHELL_CMD_ARG(recv, NULL, "Read RX buffer [timeout_s]", cmd_serial_receive, 1, 1),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(serial, &sub_serial, "Serial interface commands", NULL);

/* Modbus commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_modbus,
	SHELL_CMD_ARG(read, NULL, "Read Modbus registers <slave> <addr> <count> [holding|input]",
		      cmd_modbus_read, 4, 1),
	SHELL_CMD_ARG(write, NULL, "Write Modbus register <slave> <addr> <value>",
		      cmd_modbus_write, 4, 0),
	SHELL_CMD_ARG(sample, NULL, "Sample configured device", cmd_modbus_sample, 1, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(modbus, &sub_modbus, "Modbus RTU commands", NULL);
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
