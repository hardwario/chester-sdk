/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_driver.h"
#include "app_config.h"
#include "app_device.h"
#include "app_modbus.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>

LOG_MODULE_REGISTER(app_driver, LOG_LEVEL_DBG);

int app_driver_sample(void)
{
	int ret;

	/* Sample device drivers (MicroSENS, etc.) */
	ret = app_device_sample_all();
	if (ret && ret != -ENODATA) {
		LOG_WRN("app_device_sample_all failed: %d", ret);
	}

	/* Sample Modbus devices if configured */
	if (g_app_config.serial_mode == APP_CONFIG_SERIAL_MODE_MODBUS) {
		ret = app_modbus_sample();
		if (ret) {
			LOG_ERR("app_modbus_sample failed: %d", ret);
		}
	}

	return 0;
}
