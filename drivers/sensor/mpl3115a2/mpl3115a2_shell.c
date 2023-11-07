/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#define DT_DRV_COMPAT nxp_mpl3115a2

LOG_MODULE_REGISTER(mpl3115a2_shell, CONFIG_SENSOR_LOG_LEVEL);

#define FOREACH_BODY(inst) DEVICE_DT_INST_GET(inst),

static const struct device *devices[] = {DT_INST_FOREACH_STATUS_OKAY(FOREACH_BODY)};

static int cmd_mpl3115a2_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	for (int i = 0; i < ARRAY_SIZE(devices); i++) {
		if (!devices[i]) {
			shell_print(shell, "device not found");
			continue;
		}

		if (!device_is_ready(devices[i])) {
			shell_print(shell, "device not ready");
			continue;
		}

		ret = sensor_sample_fetch(devices[i]);
		if (ret) {
			shell_print(shell, "command failed");
			LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
			return ret;
		}

		struct sensor_value val;

		ret = sensor_channel_get(devices[i], SENSOR_CHAN_ALTITUDE, &val);
		if (ret) {
			shell_print(shell, "command failed");
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			return ret;
		}

		shell_print(shell, "Altitude: %.2lf", sensor_value_to_double(&val));

		ret = sensor_channel_get(devices[i], SENSOR_CHAN_PRESS, &val);
		if (ret) {
			shell_print(shell, "command failed");
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			return ret;
		}

		shell_print(shell, "Pressure: %.2lf", sensor_value_to_double(&val));

		ret = sensor_channel_get(devices[i], SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret) {
			shell_print(shell, "command failed");
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			return ret;
		}

		shell_print(shell, "Temperature: %.2lf", sensor_value_to_double(&val));
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
        sub_mpl3115a2,

        SHELL_CMD_ARG(read, NULL,
                      "Read sensor data",
                      cmd_mpl3115a2_read, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(mpl3115a2, &sub_mpl3115a2, "MPL3115A2 commands.", print_help);

/* clang-format on */