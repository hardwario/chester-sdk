/* CHESTER includes */
#include <chester/ctr_ds18b20.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_ds18b20_shell, CONFIG_CTR_DS18B20_LOG_LEVEL);

static int cmd_ds18b20_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int count = ctr_ds18b20_get_count();

	for (int i = 0; i < count; i++) {
		uint64_t serial_number;
		float temperature;
		ret = ctr_ds18b20_read(i, &serial_number, &temperature);
		if (ret) {
			LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell, "%llu: temperature: %.2f C", serial_number, temperature);
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
	sub_ds18b20,

	SHELL_CMD_ARG(read, NULL,
	              "Read DS18B20 thermometers.",
	              cmd_ds18b20_read, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ds18b20, &sub_ds18b20, "DS18B20 commands.", print_help);

/* clang-format on */
