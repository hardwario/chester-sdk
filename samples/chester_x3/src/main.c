/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <inttypes.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		ret = ctr_x3_set_power(dev, CTR_X3_CHANNEL_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(100));

		int32_t result;
		ret = ctr_x3_measure(dev, CTR_X3_CHANNEL_1, &result);
		if (ret) {
			LOG_ERR("Call `ctr_x3_measure` failed: %d", ret);
			k_oops();
		}

		LOG_INF("ADC result: %" PRId32 " (0x08%" PRIx32 ")", result, result);

		ret = ctr_x3_set_power(dev, CTR_X3_CHANNEL_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
