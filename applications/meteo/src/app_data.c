/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>

struct app_data g_app_data = {
	.system_voltage_rest = NAN,
	.system_voltage_load = NAN,
	.system_current_load = NAN,
	.accel_acceleration_x = NAN,
	.accel_acceleration_y = NAN,
	.accel_acceleration_z = NAN,
	.accel_orientation = INT_MAX,
	.therm_temperature = NAN,
#if defined(CONFIG_CTR_BLE_TAG)
	.ble_tag.sensor[0 ... CTR_BLE_TAG_COUNT - 1] =
		{
			.last_sample_temperature = NAN,
			.last_sample_humidity = NAN,
		},
#endif /* defined(CONFIG_CTR_BLE_TAG) */
};

static K_MUTEX_DEFINE(m_lock);

void app_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}
