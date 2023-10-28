/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x9.h>

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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x9_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_2, true);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_2, false);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_3, true);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_3, false);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_4, true);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));

		ret = ctr_x9_set_output(dev, CTR_X9_OUTPUT_4, false);
		if (ret) {
			LOG_ERR("Call `ctr_x9_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
