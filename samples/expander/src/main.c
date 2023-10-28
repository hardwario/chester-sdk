/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_b1_tca9534a));

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	bool state = false;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = gpio_pin_configure(dev, 5, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		/* Invert state variable */
		state = !state;

		/* Control LED */
		ctr_led_set(CTR_LED_CHANNEL_R, state);

		ret = gpio_pin_set(dev, 5, state ? 1 : 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
			k_oops();
		}

		/* Wait 500 ms */
		k_sleep(K_MSEC(500));
	}

	return 0;
}
