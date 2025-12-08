/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <math.h>

LOG_MODULE_REGISTER(app_backup, LOG_LEVEL_DBG);

#if defined(CONFIG_SHIELD_CTR_Z)

int app_backup_sample(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto error;
	}

	struct ctr_z_status status;
	ret = ctr_z_get_status(dev, &status);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
		goto error;
	}

	LOG_INF("DC input connected: %d", status.dc_input_connected);

	uint16_t vdc;
	ret = ctr_z_get_vdc_mv(dev, &vdc);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage DC input: %u mV", vdc);

	uint16_t vbat;
	ret = ctr_z_get_vbat_mv(dev, &vbat);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage backup battery: %u mV", vbat);

	app_data_lock();
	g_app_data.backup.line_present = status.dc_input_connected;
	g_app_data.backup.line_voltage = vdc / 1000.f;
	g_app_data.backup.battery_voltage = vbat / 1000.f;
	app_data_unlock();

	return 0;

error:
	app_data_lock();
	g_app_data.backup.line_present = false;
	g_app_data.backup.line_voltage = NAN;
	g_app_data.backup.battery_voltage = NAN;
	app_data_unlock();

	return ret;
}

int app_backup_clear(void)
{
	app_data_lock();
	g_app_data.backup.event_count = 0;
	app_data_unlock();

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_Z) */
