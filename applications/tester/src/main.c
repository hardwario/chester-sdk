/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_batt.h>
#include <ctr_gpio.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_hygro.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <logging/log.h>
#include <shell/shell.h>
#include <sys/byteorder.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		k_sleep(K_MSEC(500));
	}
}

static int cmd_gpio_test(const struct shell *shell, size_t argc, char **argv)
{
	ctr_gpio_configure(CTR_GPIO_CHANNEL_A0, CTR_GPIO_MODE_OUTPUT);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_B0, CTR_GPIO_MODE_OUTPUT);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_A2, CTR_GPIO_MODE_OUTPUT);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_B2, CTR_GPIO_MODE_OUTPUT);

	ctr_gpio_configure(CTR_GPIO_CHANNEL_A1, CTR_GPIO_MODE_INPUT_PD);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_B1, CTR_GPIO_MODE_INPUT_PD);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_A3, CTR_GPIO_MODE_INPUT_PD);
	ctr_gpio_configure(CTR_GPIO_CHANNEL_B3, CTR_GPIO_MODE_INPUT_PD);

	int pin_a1, pin_b1, pin_a3, pin_b3;

	ctr_gpio_read(CTR_GPIO_CHANNEL_A1, &pin_a1);
	ctr_gpio_read(CTR_GPIO_CHANNEL_B1, &pin_b1);
	ctr_gpio_read(CTR_GPIO_CHANNEL_A3, &pin_a3);
	ctr_gpio_read(CTR_GPIO_CHANNEL_B3, &pin_b3);

	// Print pin state, should be all zeros
	shell_print(shell, "off: A1 %u, B1 %u, A3 %u, B3 %u", pin_a1, pin_b1, pin_a3, pin_b3);

	// Enable outputs
	ctr_gpio_write(CTR_GPIO_CHANNEL_A0, 1);
	ctr_gpio_write(CTR_GPIO_CHANNEL_B0, 1);
	ctr_gpio_write(CTR_GPIO_CHANNEL_A2, 1);
	ctr_gpio_write(CTR_GPIO_CHANNEL_B2, 1);

	ctr_gpio_read(CTR_GPIO_CHANNEL_A1, &pin_a1);
	ctr_gpio_read(CTR_GPIO_CHANNEL_B1, &pin_b1);
	ctr_gpio_read(CTR_GPIO_CHANNEL_A3, &pin_a3);
	ctr_gpio_read(CTR_GPIO_CHANNEL_B3, &pin_b3);

	// Print pin state, should be all ones
	shell_print(shell, "on: A1 %u, B1 %u, A3 %u, B3 %u", pin_a1, pin_b1, pin_a3, pin_b3);

	// Turn off outputs
	ctr_gpio_write(CTR_GPIO_CHANNEL_A0, 0);
	ctr_gpio_write(CTR_GPIO_CHANNEL_B0, 0);
	ctr_gpio_write(CTR_GPIO_CHANNEL_A2, 0);
	ctr_gpio_write(CTR_GPIO_CHANNEL_B2, 0);

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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio,
                               SHELL_CMD_ARG(test, NULL, "Test X module GPIO.", cmd_gpio_test, 0,
                                             0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(gpio, &sub_gpio, "GPIO commands.", print_help);
