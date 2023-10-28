/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x10.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x10));

void ctr_x10_event_handler(const struct device *dev, enum ctr_x10_event ev, void *user_data)
{
	switch (ev) {
	case CTR_X10_EVENT_LINE_CONNECTED:
		LOG_INF("Line connected");
		break;
	case CTR_X10_EVENT_LINE_DISCONNECTED:
		LOG_INF("Line disconnected");
		break;
	default:
		LOG_ERR("Unknown event: %d", ev);
		k_oops();
	}
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_x10_set_handler(dev, ctr_x10_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_x10_set_handler` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		int battery_voltage_mv;
		ret = ctr_x10_get_battery_voltage(dev, &battery_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_battery_voltage` failed: %d", ret);
		} else {
			LOG_INF("Battery voltage: %d mv", battery_voltage_mv);
		}

		int line_voltage_mv;
		ret = ctr_x10_get_line_voltage(dev, &line_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_line_voltage` failed: %d", ret);
		} else {
			LOG_INF("Line voltage: %d mv", line_voltage_mv);
		}

		bool line_present;
		ret = ctr_x10_get_line_present(dev, &line_present);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_line_present` failed: %d", ret);
		} else {
			LOG_INF("Line present: %d", line_present);
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
