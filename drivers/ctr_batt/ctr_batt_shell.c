/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_batt.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_DECLARE(ctr_batt, CONFIG_CTR_BATT_LOG_LEVEL);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `CTR_BATT` not ready");
		shell_error(shell, "command failed");
		return -EINVAL;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(m_dev, &rest_mv, CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(m_dev, &load_mv, CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	int load_ma;
	ctr_batt_get_load_current_ma(m_dev, &load_ma, load_mv);

	shell_print(shell, "rest voltage: %u millivolts", rest_mv);
	shell_print(shell, "load voltage: %u millivolts", load_mv);
	shell_print(shell, "load current: %u milliamps", load_ma);

	return 0;
}

static int cmd_load(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `CTR_BATT` not ready");
		shell_error(shell, "command failed");
		return -EINVAL;
	}

	ret = ctr_batt_load(m_dev);
	if (ret) {
		LOG_ERR("Call `ctr_batt_load` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_unload(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `CTR_BATT` not ready");
		shell_error(shell, "command failed");
		return -EINVAL;
	}

	ret = ctr_batt_unload(m_dev);
	if (ret) {
		LOG_ERR("Call `ctr_batt_unload` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_batt,

	SHELL_CMD_ARG(measure, NULL, "Measure battery voltage.", cmd_measure, 1, 0),
        SHELL_CMD_ARG(load, NULL, "Manually activate battery load.", cmd_load, 1, 0),
        SHELL_CMD_ARG(unload, NULL, "Manually deactivate battery load.", cmd_unload, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(batt, &sub_batt, "Battery commands.", NULL);

/* clang-format on */
