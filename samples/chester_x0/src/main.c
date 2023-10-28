/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_1, CTR_X0_MODE_NPN_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_2, CTR_X0_MODE_PNP_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_3, CTR_X0_MODE_CL_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_4, CTR_X0_MODE_PWR_SOURCE);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");
		k_sleep(K_SECONDS(10));
	}

	return 0;
}
