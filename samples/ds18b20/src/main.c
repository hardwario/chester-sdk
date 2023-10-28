/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		int count = ctr_ds18b20_get_count();

		for (int i = 0; i < count; i++) {
			uint64_t serial_number;
			float temperature;
			ret = ctr_ds18b20_read(i, &serial_number, &temperature);
			if (ret) {
				LOG_ERR("Call `ctr_ds18b20_read` failed: %d", ret);
			} else {
				LOG_INF("Serial number: %llu / Temperature: %.2f C", serial_number,
					temperature);
			}
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
