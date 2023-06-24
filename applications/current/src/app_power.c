/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "app_power.h"

/* CHESTER includes */
#include <chester/drivers/ctr_batt.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <math.h>

LOG_MODULE_REGISTER(app_power, LOG_LEVEL_DBG);

int app_power_sample(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto error;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv, CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
		goto error;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(dev, &load_mv, CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
		goto error;
	}

	int current_ma;
	ctr_batt_get_load_current_ma(dev, &current_ma, load_mv);

	LOG_INF("Battery voltage (rest): %d mV", rest_mv);
	LOG_INF("Battery voltage (load): %d mV", load_mv);
	LOG_INF("Battery current (load): %d mA", current_ma);

	app_data_lock();

	g_app_data.system_voltage_rest = rest_mv;
	g_app_data.system_voltage_load = load_mv;
	g_app_data.system_current_load = current_ma;

	app_data_unlock();

	return 0;

error:
	app_data_lock();

	g_app_data.system_voltage_rest = NAN;
	g_app_data.system_voltage_load = NAN;
	g_app_data.system_current_load = NAN;

	app_data_unlock();

	return ret;
}
