/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ds18b20.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ctr_ds18b20_shell, CONFIG_CTR_DS18B20_LOG_LEVEL);

static int cmd_ds18b20_scan(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_ds18b20_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int repetitions = 1;

	if (argc == 2) {
		repetitions = strtoll(argv[1], NULL, 10);

		if (repetitions < 1 || repetitions > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}
	}

	int count = ctr_ds18b20_get_count();

	shell_print(shell, "device count: %d", count);

	for (int i = 0; i < repetitions; i++) {
		if (i) {
			k_sleep(K_SECONDS(1));
		}

		if (repetitions > 1) {
			shell_print(shell, "repetition: %d/%d", i + 1, repetitions);
		}

		for (int j = 0; j < count; j++) {
			uint64_t serial_number;
			float temperature;
			ret = ctr_ds18b20_read(j, &serial_number, &temperature);
			if (ret) {
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
				shell_error(shell, "command failed");
				return ret;
			}

			shell_print(shell, "serial number %llu: temperature: %.2f C", serial_number,
				    temperature);
		}
	}

	shell_print(shell, "command succeeded");

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

	SHELL_CMD_ARG(scan, NULL,
	              "Scan DS18B20 thermometers.",
	              cmd_ds18b20_scan, 1, 0),

	SHELL_CMD_ARG(read, NULL,
	              "Read DS18B20 thermometers "
	              "w/ optional number of repetitions (format: [<1-3600>]).",
	              cmd_ds18b20_read, 1, 1),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ds18b20, &sub_ds18b20, "DS18B20 commands.", print_help);

/* clang-format on */
