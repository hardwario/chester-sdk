/*
 * Copyright (c) 2023 HARDWARIO a.s.
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

/*
 * Keep `g_app_data` zero-initialized so it is placed in `.bss` (RAM only).
 * Non-zero initializers (NAN, INT_MAX) would move the whole structure to
 * `.data`, which occupies the same amount of FLASH and is copied on boot.
 * Non-zero defaults are set at runtime in `app_data_init()` instead.
 */
struct app_data g_app_data;

static K_MUTEX_DEFINE(m_lock);

int app_data_init(void)
{
	app_data_lock();

	g_app_data.accel_acceleration_x = NAN;
	g_app_data.accel_acceleration_y = NAN;
	g_app_data.accel_acceleration_z = NAN;
	g_app_data.accel_orientation = INT_MAX;
	g_app_data.therm_temperature = NAN;

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		g_app_data.ble_tag.sensor[i].last_sample_temperature = NAN;
		g_app_data.ble_tag.sensor[i].last_sample_humidity = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

	app_data_unlock();

	return 0;
}

void app_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}

K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
