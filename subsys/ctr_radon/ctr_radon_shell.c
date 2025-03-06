#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#include <chester/ctr_radon.h>

#include "ctr_radon_config.h"

LOG_MODULE_REGISTER(ctr_radon_shell, CONFIG_CTR_RADON_LOG_LEVEL);

static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = ctr_radon_enable();
	if (ret) {
		shell_error(shell, "Call `ctr_radon_enable` failed: %d", ret);
		return ret;
	}

	int16_t temperature;
	ret = ctr_radon_read_temperature(&temperature);
	if (ret) {
		shell_error(shell, "Call `ctr_radon_read_temperature` failed: %d", ret);
		return ret;
	}

	uint16_t humidity;
	ret = ctr_radon_read_humidity(&humidity);
	if (ret) {
		shell_error(shell, "Call `ctr_radon_read_humidity` failed: %d", ret);
		return ret;
	}

	uint32_t concentration;
	ret = ctr_radon_read_concentration(&concentration, false);
	if (ret) {
		shell_error(shell, "Call `ctr_radon_read_concentration` failed: %d", ret);
		return ret;
	}

	uint32_t concentration_daily;
	ret = ctr_radon_read_concentration(&concentration_daily, true);
	if (ret) {
		shell_error(shell, "Call `ctr_radon_read_concentration` failed: %d", ret);
		return ret;
	}

	shell_print(shell, "temperature: %d", temperature);
	shell_print(shell, "humidity: %d", humidity);
	shell_print(shell, "concentration: %d", concentration);
	shell_print(shell, "concentration_daily: %d", concentration_daily);

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
	sub_radon,

	SHELL_CMD_ARG(config, NULL,
	              "Configuration commands.",
	              ctr_radon_config_cmd, 1, 3),
	
	SHELL_CMD_ARG(read, NULL, "Read radon probe", cmd_read, 1, 0),

	SHELL_SUBCMD_SET_END
);
/* clang-format on */

SHELL_CMD_REGISTER(radon, &sub_radon, "Radon commands.", print_help);