#include "app_config.h"
#include "app_loop.h"

/* Zephyr includes */
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_DBG);

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	atomic_set(&g_app_loop_send, true);
	k_sem_give(&g_app_loop_sem);

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
	sub_app_config,

	SHELL_CMD_ARG(show, NULL,
	              "List current configuration.",
	              app_config_cmd_config_show, 1, 0),

	SHELL_CMD_ARG(measurement-interval, NULL,
	              "Get/Set measurement interval in seconds (format: <5-3600>).",
	              app_config_cmd_config_measurement_interval, 1, 1),

	SHELL_CMD_ARG(report-interval, NULL,
	              "Get/Set report interval in seconds (format: <30-86400>).",
	              app_config_cmd_config_report_interval, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_app,

	SHELL_CMD_ARG(config, &sub_app_config, "Configuration commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);

/* clang-format on */
