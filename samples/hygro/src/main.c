/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_hygro.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		float temperature;
		float humidity;
		ret = ctr_hygro_read(&temperature, &humidity);
		if (ret) {
			LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);

		} else {
			LOG_INF("Temperature: %.2f C", temperature);
			LOG_INF("Humidity: %.1f %%", humidity);
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
