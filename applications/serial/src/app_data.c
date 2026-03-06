/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <math.h>

/* Global application data */
struct app_data g_app_data = {
#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	.ble_tag.sensor[0 ... CTR_BLE_TAG_COUNT - 1] = {
		.last_sample_temperature = NAN,
		.last_sample_humidity = NAN,
		.voltage = NAN,
	},
#endif
};

/* Mutex for thread-safe access */
static K_MUTEX_DEFINE(m_data_mutex);

void app_data_lock(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_data_mutex);
}
