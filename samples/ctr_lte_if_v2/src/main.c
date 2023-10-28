/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_lte_if_v2.h>
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(ctr_lte_if_v2));

static void recv_cb(const struct device *dev, enum ctr_lte_if_v2_event event, void *user_data)
{
	int ret;

	switch (event) {
	case CTR_LTE_IF_V2_EVENT_RX_LINE:
		LOG_INF("Event `CTR_LTE_IF_V2_EVENT_RX_LINE`");
		for (;;) {
			char *line;
			ret = ctr_lte_if_v2_recv_line(dev, K_NO_WAIT, &line);
			if (ret) {
				LOG_ERR("Call `ctr_lte_if_v2_recv_line` failed: %d", ret);
				break;
			}

			if (!line) {
				break;
			}

			LOG_INF("RX line: %s", line);

			ret = ctr_lte_if_v2_free_line(dev, line);
			if (ret) {
				LOG_ERR("Call `ctr_lte_if_v2_free_line` failed: %d", ret);
			}
		}
		break;
	case CTR_LTE_IF_V2_EVENT_RX_DATA:
		LOG_INF("Event `CTR_LTE_IF_V2_EVENT_RX_DATA`");
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static int init_rfmux(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_rfmux_acquire(dev);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_acquire` failed: %d", ret);
		return ret;
	}

	ret = ctr_rfmux_set_interface(dev, CTR_RFMUX_INTERFACE_LTE);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_interface` failed (LTE): %d", ret);
		return ret;
	}

	ret = ctr_rfmux_set_antenna(dev, CTR_RFMUX_ANTENNA_INT);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_antenna` failed (INT): %d", ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = init_rfmux();
	if (ret) {
		LOG_ERR("Call `init_rfmux` failed: %d", ret);
		k_oops();
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_lte_if_v2_set_callback(dev, recv_cb, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_set_callback` failed: %d", ret);
		k_oops();
	}

	return 0;
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_reset(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_wakeup(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_wake_up(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_wake_up` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_enable(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_enable_uart(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_enable_uart` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_disable(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_disable_uart(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_disable_uart` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_enterdata(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_enter_data_mode(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_enter_data_mode` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_exitdata(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_exit_data_mode(dev);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_exit_data_mode` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc < 2) {
		shell_error(shell, "missing argument");
		shell_help(shell);
		return -EINVAL;
	}

	if (argc > 2) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lte_if_v2_send_line(dev, K_FOREVER, "%s", argv[1]);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_v2_send_line` failed: %d", ret);
		return ret;
	}

	return 0;
}

SHELL_CMD_REGISTER(reset, NULL, "Reset modem.", cmd_reset);
SHELL_CMD_REGISTER(wakeup, NULL, "Wake up modem.", cmd_wakeup);
SHELL_CMD_REGISTER(enable, NULL, "Enable modem UART.", cmd_enable);
SHELL_CMD_REGISTER(disable, NULL, "Disable modem UART.", cmd_disable);
SHELL_CMD_REGISTER(enterdata, NULL, "Enter data mode.", cmd_enterdata);
SHELL_CMD_REGISTER(exitdata, NULL, "Exit data mode.", cmd_exitdata);

SHELL_CMD_REGISTER(send, NULL, "Send command.", cmd_send);
