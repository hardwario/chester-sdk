/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_a1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_a1));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_a1_set_relay(dev, CTR_A1_RELAY_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_relay` failed: %d", ret);
			k_oops();
		}

		ret = ctr_a1_set_led(dev, CTR_A1_LED_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_led` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_a1_set_relay(dev, CTR_A1_RELAY_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_relay` failed: %d", ret);
			k_oops();
		}

		ret = ctr_a1_set_led(dev, CTR_A1_LED_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_led` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_a1_set_relay(dev, CTR_A1_RELAY_2, true);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_relay` failed: %d", ret);
			k_oops();
		}

		ret = ctr_a1_set_led(dev, CTR_A1_LED_2, true);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_led` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(2));

		ret = ctr_a1_set_relay(dev, CTR_A1_RELAY_2, false);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_relay` failed: %d", ret);
			k_oops();
		}

		ret = ctr_a1_set_led(dev, CTR_A1_LED_2, false);
		if (ret) {
			LOG_ERR("Call `ctr_a1_set_led` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(2));
	}

	return 0;
}
