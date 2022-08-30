#include "app_config.h"
#include "app_measure.h"
#include "app_send.h"

/* Zephyr includes */
#include <drivers/ctr_s1.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdlib.h>

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&g_app_measure_timer, K_NO_WAIT, K_FOREVER);

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&g_app_send_timer, K_NO_WAIT, K_FOREVER);

	return 0;
}

static int cmd_calibrate(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 2) {
		int co2_target_ppm = strtol(argv[1], NULL, 10);

		if (co2_target_ppm < 0) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));
		if (!device_is_ready(dev)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		ret = ctr_s1_calibrate_target_co2_concentration(dev, co2_target_ppm);
		if (ret) {
			LOG_ERR("Call `ctr_s1_calibrate_target_co2_concentration` failed: %d", ret);
			return ret;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_debug(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&g_app_measure_timer, K_NO_WAIT, K_FOREVER);

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

SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(debug, NULL, "Debug command.", cmd_debug);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);

#if defined(CONFIG_SHIELD_CTR_S1)
SHELL_CMD_REGISTER(calibrate, NULL, "Calibrate CO2.", cmd_calibrate);
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

/* clang-format on */
