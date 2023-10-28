/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <chester/drivers/ctr_batt.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `CTR_BATT` not ready");
		k_oops();
	}

	for (;;) {
		int rest_mv;
		ret = ctr_batt_get_rest_voltage_mv(m_dev, &rest_mv,
						   CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
			k_oops();
		}

		int load_mv;
		ret = ctr_batt_get_load_voltage_mv(m_dev, &load_mv,
						   CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
			k_oops();
		}

		int current_ma;
		ctr_batt_get_load_current_ma(m_dev, &current_ma, load_mv);

		LOG_INF("Battery rest voltage = %u mV", rest_mv);
		LOG_INF("Battery load voltage = %u mV", load_mv);
		LOG_INF("Battery load current = %u mA", current_ma);

		k_sleep(K_SECONDS(60));
	}

	return 0;
}
