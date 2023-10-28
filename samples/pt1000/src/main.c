/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_rtd.h>

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

	for (;;) {
		float temperature;
		ret = ctr_rtd_read(CTR_RTD_CHANNEL_A1, CTR_RTD_TYPE_PT1000, &temperature);
		if (ret) {
			LOG_ERR("Call `ctr_rtd_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Temperature: %.3f C", temperature);

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
