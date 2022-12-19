/* CHESTER includes */
#include <chester/ctr_button.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_button, CONFIG_CTR_BUTTON_LOG_LEVEL);

static const struct gpio_dt_spec m_button_int = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec m_button_ext = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static int read_button(enum ctr_button_channel channel, bool *is_pressed)
{
	int ret;

	switch (channel) {
	case CTR_BUTTON_CHANNEL_INT:
		ret = gpio_pin_get_dt(&m_button_int);
		break;

	case CTR_BUTTON_CHANNEL_EXT:
		ret = gpio_pin_get_dt(&m_button_ext);
		break;

	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_get_dt` failed: %d", ret);
		return ret;
	}

	*is_pressed = ret == 0 ? false : true;

	return 0;
}

static int cmd_button_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum ctr_button_channel channel;

	if (strcmp(argv[1], "int") == 0) {
		channel = CTR_BUTTON_CHANNEL_INT;

	} else if (strcmp(argv[1], "ext") == 0) {
		channel = CTR_BUTTON_CHANNEL_EXT;

	} else {
		shell_error(shell, "invalid channel name");
		shell_help(shell);
		return -EINVAL;
	}

	bool is_pressed;
	ret = read_button(channel, &is_pressed);
	if (ret) {
		LOG_ERR("Call `read_button` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "pressed: %s", is_pressed ? "true" : "false");

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
	sub_button,

	SHELL_CMD_ARG(read, NULL,
	              "Read button state (format int|ext).",
	              cmd_button_read, 2, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(button, &sub_button, "Button commands.", print_help);

/* clang-format on */

static int init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	if (!device_is_ready(m_button_int.port)) {
		LOG_ERR("Device `BUTTON_INT` not ready");
		return -EINVAL;
	}

	if (!device_is_ready(m_button_ext.port)) {
		LOG_ERR("Device `BUTTON_EXT` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&m_button_int, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&m_button_ext, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_BUTTON_INIT_PRIORITY);
