/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/people_counter.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(people_counter));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = people_counter_set_power_off_delay(dev, 30);
	if (ret) {
		LOG_ERR("Call `people_counter_set_power_off_delay` failed: %d", ret);
		k_oops();
	}

	ret = people_counter_set_stay_timeout(dev, 5);
	if (ret) {
		LOG_ERR("Call `people_counter_set_stay_timeout` failed: %d", ret);
		k_oops();
	}

	ret = people_counter_set_adult_border(dev, 4);
	if (ret) {
		LOG_ERR("Call `people_counter_set_adult_border` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		struct people_counter_measurement measurement;
		ret = people_counter_read_measurement(dev, &measurement);
		if (ret) {
			LOG_ERR("Call `people_counter_read_measurement` failed: %d", ret);
			k_oops();
		} else {
			LOG_INF("Motion counter: %u", measurement.motion_counter);
			LOG_INF("Pass counter (adult): %u", measurement.pass_counter_adult);
			LOG_INF("Pass counter (child): %u", measurement.pass_counter_child);
			LOG_INF("Stay counter (adult): %u", measurement.stay_counter_adult);
			LOG_INF("Stay counter (child): %u", measurement.stay_counter_child);
			LOG_INF("Total time (adult): %u", measurement.total_time_adult);
			LOG_INF("Total time (child): %u", measurement.total_time_child);
			LOG_INF("Consumed energy: %u", measurement.consumed_energy);
		}

		int value;

		ret = people_counter_get_power_off_delay(dev, &value);
		if (ret) {
			LOG_ERR("Call `people_counter_get_power_off_delay` failed: %d", ret);
			k_oops();
		} else {
			LOG_INF("Power off delay: %d", value);
		}

		ret = people_counter_get_stay_timeout(dev, &value);
		if (ret) {
			LOG_ERR("Call `people_counter_get_stay_timeout` failed: %d", ret);
			k_oops();
		} else {
			LOG_INF("Stay timeout: %d", value);
		}

		ret = people_counter_get_adult_border(dev, &value);
		if (ret) {
			LOG_ERR("Call `people_counter_get_adult_border` failed: %d", ret);
			k_oops();
		} else {
			LOG_INF("Adult border: %d", value);
		}

		k_sleep(K_MSEC(500));
	}

	return 0;
}
