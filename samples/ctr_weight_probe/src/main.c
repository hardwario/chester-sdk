/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_weight_probe.h>

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

	ret = ctr_weight_probe_scan();
	if (ret) {
		LOG_ERR("Call `ctr_weight_probe_scan` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		int count = ctr_weight_probe_get_count();

		for (int i = 0; i < count; i++) {
			uint64_t serial_number;
			int32_t result;
			ret = ctr_weight_probe_read(i, &serial_number, &result);
			if (ret) {
				LOG_ERR("Call `ctr_weight_probe_read` failed: %d", ret);
			} else {
				LOG_INF("Serial number: %llu / Result: %d", serial_number, result);
			}
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
