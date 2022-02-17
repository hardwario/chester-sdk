#include <ctr_led.h>

/* Zephyr includes */
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_led_shell, CONFIG_CTR_LED_LOG_LEVEL);

static int cmd_led_switch(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum ctr_led_channel channel;

	if (strcmp(argv[1], "red") == 0) {
		channel = CTR_LED_CHANNEL_R;

	} else if (strcmp(argv[1], "green") == 0) {
		channel = CTR_LED_CHANNEL_G;

	} else if (strcmp(argv[1], "yellow") == 0) {
		channel = CTR_LED_CHANNEL_Y;

	} else if (strcmp(argv[1], "ext") == 0) {
		channel = CTR_LED_CHANNEL_EXT;

	} else {
		shell_error(shell, "invalid channel name");
		shell_help(shell);
		return -EINVAL;
	}

	if (strcmp(argv[2], "on") == 0) {
		ret = set_led(channel, true);

	} else if (strcmp(argv[2], "off") == 0) {
		ret = set_led(channel, false);

	} else {
		shell_error(shell, "invalid command");
		shell_help(shell);
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `set_led` failed: %d", ret);
		return ret;
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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_led,
                               SHELL_CMD_ARG(switch, NULL,
                                             "Switch LED channel"
                                             " (format red|green|yellow|ext on|off).",
                                             cmd_led_switch, 3, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(led, &sub_led, "LED commands.", print_help);
