#include "mock.h"

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

/* Chester includes */
#include <chester/ctr_config.h>

struct app_config {
	int interval_sample;
	int interval_report;
	bool backup_report_connected;
};

static struct app_config m_app_config_interim = {
	.interval_sample = 5,
	.interval_report = 60,
	.backup_report_connected = false,
};

// void sys_arch_reboot(int type)
// {
// 	printf("sys_arch_reboot: %d\n", type);
// }

static int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "app config interval-sample %d", m_app_config_interim.interval_sample);
	shell_print(shell, "app config interval-report %d", m_app_config_interim.interval_report);
	shell_print(shell, "app config backup-report-connected %s",
		    m_app_config_interim.backup_report_connected ? "true" : "false");
	return 0;
}

void *mock_global_setup_suite(void)
{
	static bool is_setup = false;

	printf("mock_global_setup_suite: %d\n", is_setup);

	if (is_setup) {
		return NULL;
	}

	// Wait for the initialization of the shell dummy backend
	const struct shell *sh = shell_backend_dummy_get_ptr();
	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
	zassert_true(shell_ready(sh), "timed out waiting for dummy shell backend");

	// Register callbacks for the `config show` command
	ctr_config_append_show("app", app_config_cmd_config_show);

	is_setup = true;

	return NULL;
}

int app_config_cmd_config_interval_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 2) {
		int interval_sample = atoi(argv[1]);

		if (interval_sample < 5 || interval_sample > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_sample = interval_sample;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 2) {
		int interval_report = atoi(argv[1]);

		if (interval_report < 30 || interval_report > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_report = interval_report;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_backup_report_connected(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 2) {
		bool is_false = !strcmp(argv[1], "false");
		bool is_true = !strcmp(argv[1], "true");

		if (is_false) {
			m_app_config_interim.backup_report_connected = false;
		} else if (is_true) {
			m_app_config_interim.backup_report_connected = true;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
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

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_app_config,

	SHELL_CMD_ARG(show, NULL, "List current configuration.", app_config_cmd_config_show, 1, 0),

	SHELL_CMD_ARG(interval-sample, NULL,
		      "Get/Set sample interval in seconds (format: <5-3600>).",
		      app_config_cmd_config_interval_sample, 1, 1),

	SHELL_CMD_ARG(interval-report, NULL,
		      "Get/Set report interval in seconds (format: <30-86400>).",
		      app_config_cmd_config_interval_report, 1, 1),

	SHELL_CMD_ARG(backup-report-connected, NULL,
		      "Get/Set report when backup is active (format: true, false).",
		      app_config_cmd_config_backup_report_connected, 1, 1),

	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app,

			       SHELL_CMD_ARG(config, &sub_app_config, "Configuration commands.",
					     print_help, 1, 0),

			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);
