#include <ctr_button.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(hio_button, LOG_LEVEL_DBG);

static const struct gpio_dt_spec m_button_int = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec m_button_ext = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static int read_button(enum hio_button_channel channel, bool *is_pressed)
{
	int ret;

	switch (channel) {
	case HIO_BUTTON_CHANNEL_INT:
		ret = gpio_pin_get_dt(&m_button_int);
		break;

	case HIO_BUTTON_CHANNEL_EXT:
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

	*is_pressed = ret == 0 ? true : false;

	return 0;
}

static int cmd_button_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum hio_button_channel channel;

	if (strcmp(argv[1], "int") == 0) {
		channel = HIO_BUTTON_CHANNEL_INT;

	} else if (strcmp(argv[1], "ext") == 0) {
		channel = HIO_BUTTON_CHANNEL_EXT;

	} else {
		shell_error(shell, "invalid channel name");
		shell_help(shell);
		return -EINVAL;
	}

	bool is_pressed;

	ret = read_button(channel, &is_pressed);

	if (ret < 0) {
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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_button,
                               SHELL_CMD_ARG(read, NULL,
                                             "Read button state"
                                             " (format int|ext).",
                                             cmd_button_read, 2, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(button, &sub_button, "Button commands.", print_help);
