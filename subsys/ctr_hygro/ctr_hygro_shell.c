/* CHESTER includes */
#include <ctr_hygro.h>

/* Zephyr includes */
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_hygro_shell, CONFIG_CTR_HYGRO_LOG_LEVEL);

static int cmd_hygro_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float temperature;
	float humidity;
	ret = ctr_hygro_read(&temperature, &humidity);
	if (ret) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "temperature: %.2f C", temperature);
	shell_print(shell, "humidity: %.1f %%", humidity);

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
	              "Read sensor data.",
		      cmd_hygro_read, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(hygro, &sub_hygro, "Hygrometer commands.", print_help);

/* clang-format on */
