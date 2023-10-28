/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = cfb_framebuffer_init(dev);
	if (ret) {
		LOG_ERR("Call `cfb_framebuffer_init` failed: %d", ret);
		k_oops();
	}

	ret = cfb_framebuffer_invert(dev);
	if (ret) {
		LOG_ERR("Call `cfb_framebuffer_invert` failed: %d", ret);
		k_oops();
	}

	ret = cfb_print(dev, "HARDWARIO", 0, 0);
	if (ret) {
		LOG_ERR("Call `cfb_print` failed: %d", ret);
		k_oops();
	}

	ret = cfb_print(dev, "CHESTER", 0, 16);
	if (ret) {
		LOG_ERR("Call `cfb_print` failed: %d", ret);
		k_oops();
	}

	ret = cfb_framebuffer_finalize(dev);
	if (ret) {
		LOG_ERR("Call `cfb_framebuffer_finalize` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = display_blanking_on(dev);
		if (ret) {
			LOG_ERR("Call `display_blanking_on` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(1000));

		ret = display_blanking_off(dev);
		if (ret) {
			LOG_ERR("Call `display_blanking_off` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
