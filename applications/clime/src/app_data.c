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

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	g_app_data.hygro.last_sample_temperature = NAN;
	g_app_data.hygro.last_sample_humidity = NAN;
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	for (int i = 0; i < APP_DATA_W1_THERM_COUNT; i++) {
		g_app_data.w1_therm.sensor[i].last_sample_temperature = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B)
	for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
		g_app_data.rtd_therm.sensor[i].last_sample_temperature = NAN;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B) */

#if defined(FEATURE_HARDWARE_CHESTER_TC_A) || defined(FEATURE_HARDWARE_CHESTER_TC_B)
	for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {
		g_app_data.tc_therm.sensor[i].last_sample_temperature = NAN;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_TC_A) || defined(FEATURE_HARDWARE_CHESTER_TC_B) */

#if defined(FEATURE_HARDWARE_CHESTER_SPS30)
	g_app_data.sps30.last_sample_mass_conc_pm_1_0 = NAN;
	g_app_data.sps30.last_sample_mass_conc_pm_2_5 = NAN;
	g_app_data.sps30.last_sample_mass_conc_pm_4_0 = NAN;
	g_app_data.sps30.last_sample_mass_conc_pm_10_0 = NAN;
	g_app_data.sps30.last_sample_num_conc_pm_0_5 = NAN;
	g_app_data.sps30.last_sample_num_conc_pm_1_0 = NAN;
	g_app_data.sps30.last_sample_num_conc_pm_2_5 = NAN;
	g_app_data.sps30.last_sample_num_conc_pm_4_0 = NAN;
	g_app_data.sps30.last_sample_num_conc_pm_10_0 = NAN;
#endif /* defined(FEATURE_HARDWARE_CHESTER_SPS30) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		g_app_data.ble_tag.sensor[i].last_sample_temperature = NAN;
		g_app_data.ble_tag.sensor[i].last_sample_humidity = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
		g_app_data.soil_sensor.sensor[i].last_temperature = NAN;
		g_app_data.soil_sensor.sensor[i].last_moisture = NAN;
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

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
