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

SHELL_DYNAMIC_CMD_CREATE(sub_app_config, app_config_subcmd_get);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app,

			       SHELL_CMD_ARG(config, &sub_app_config, "Configuration commands.",
					     app_config_cmd_config, 1, 3),

/* ### Preserved code "subcmd" (begin) */
/* ^^^ Preserved code "subcmd" (end) */

			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);

SHELL_CMD_REGISTER(sample, NULL, "Sample immediately.", cmd_sample);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);

/* ### Preserved code "functions 2" (begin) */
/* Serial commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_serial,
	SHELL_CMD_ARG(send, NULL, "Send hex data and print response <hex> [timeout_s] (default 1).", cmd_serial_send, 2, 1),
	SHELL_CMD_ARG(recv, NULL, "Receive data and print it [timeout_s] (default 1).", cmd_serial_receive, 1, 1),
	SHELL_CMD_ARG(pm, NULL, "Suspend or resume UART bridge <0|1> (0: suspend, 1: resume).", cmd_serial_pm, 2, 0),
	SHELL_CMD_ARG(break, NULL, "Hold TX low or release it to idle high <0|1> (1: hold low, 0: release).", cmd_serial_break, 2, 0),
	SHELL_CMD_ARG(reinit, NULL, "Re-apply UART configuration.", cmd_serial_reinit, 1, 0),
	SHELL_CMD_ARG(wake, NULL, "Wake peer, send hex data and print response <hex> [timeout_s] [high_ms] (defaults 6 s, 2500 ms).", cmd_serial_wake_send, 2, 2),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(serial, &sub_serial, "Serial interface commands.", NULL);

/* Modbus commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_modbus,
	SHELL_CMD_ARG(read, NULL, "Read Modbus registers <slave> <addr> <count> [holding|input] (count: 1-32, default input).",
		      cmd_modbus_read, 4, 1),
	SHELL_CMD_ARG(write, NULL, "Write Modbus holding register <slave> <addr> <value>.",
		      cmd_modbus_write, 4, 0),
	SHELL_CMD_ARG(sample, NULL, "Sample configured devices without a driver (lambrecht, generic).",
		      cmd_modbus_sample, 1, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(modbus, &sub_modbus, "Modbus RTU commands.", NULL);
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
