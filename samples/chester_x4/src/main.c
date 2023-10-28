/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x4.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void ctr_x4_handler(const struct device *dev, enum ctr_x4_event event, void *user_data)
{
	switch (event) {
	case CTR_X4_EVENT_LINE_CONNECTED:
		LOG_INF("Line connected");
		break;
	case CTR_X4_EVENT_LINE_DISCONNECTED:
		LOG_INF("Line disconnected");
		break;
	default:
		LOG_ERR("Unknown event: %d", event);
		k_oops();
	}
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_x4_set_handler(dev, ctr_x4_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_x4_set_handler` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		int line_voltage_mv;
		ret = ctr_x4_get_line_voltage(dev, &line_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x4_get_line_voltage` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Line voltage: %d mV", line_voltage_mv);

		bool is_line_present;
		ret = ctr_x4_get_line_present(dev, &is_line_present);
		if (ret) {
			LOG_ERR("Call `ctr_x4_get_line_present` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Line present: %s", is_line_present ? "true" : "false");

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
