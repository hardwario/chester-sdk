/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_meteo.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_meteo_shell, CONFIG_CTR_METEO_LOG_LEVEL);

static int cmd_meteo_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	const struct device *dev = device_get_binding(argv[1]);
	if (!dev) {
		LOG_ERR("Device not found");
		shell_error(shell, "device not found");
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		shell_error(shell, "device not ready");
		return -ENODEV;
	}

	float rainfall;
	ret = ctr_meteo_get_rainfall_and_clear(dev, &rainfall);
	if (ret) {
		LOG_ERR("Call `ctr_meteo_get_rainfall_and_clear` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "rainfall: %.2f mm", rainfall);

	float speed;
	ret = ctr_meteo_get_wind_speed_and_clear(dev, &speed);
	if (ret) {
		LOG_ERR("Call `ctr_meteo_get_wind_speed_and_clear` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "wind speed: %.2f m/s", speed);

	float direction;
	ret = ctr_meteo_get_wind_direction(dev, &direction);
	if (ret) {
		LOG_ERR("Call `ctr_meteo_get_wind_direction` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "wind direction: %.2f deg", direction);

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
	sub_meteo,

	SHELL_CMD_ARG(read, NULL,
	              "Read sensor data (format: <device>).",
	              cmd_meteo_read, 2, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(meteo, &sub_meteo, "Meteo commands.", print_help);

/* clang-format on */
