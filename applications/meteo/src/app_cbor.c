/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_codec.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

__unused static void put_sample_mul(zcbor_state_t *zs, struct app_data_aggreg *sample, float mul)
{
	if (isnan(sample->min)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->min * mul);
	}

	if (isnan(sample->max)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->max * mul);
	}

	if (isnan(sample->avg)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->avg * mul);
	}

	if (isnan(sample->mdn)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->mdn * mul);
	}
}

__unused static void put_sample(zcbor_state_t *zs, struct app_data_aggreg *sample)
{
	put_sample_mul(zs, sample, 1.f);
}

static int encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__VERSION);
		zcbor_uint32_put(zs, 1);

		static uint32_t sequence = 0;
		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__SEQUENCE);
		zcbor_uint32_put(zs, sequence++);

		uint64_t timestamp;
		ret = ctr_rtc_get_ts(&timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__TIMESTAMP);
		zcbor_uint64_put(zs, timestamp);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__UPTIME);
		zcbor_uint64_put(zs, k_uptime_get() / 1000);

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__VOLTAGE_REST);
		if (isnan(g_app_data.system_voltage_rest)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_rest);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__VOLTAGE_LOAD);
		if (isnan(g_app_data.system_voltage_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_load);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__CURRENT_LOAD);
		if (isnan(g_app_data.system_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__VENDOR_NAME);
		zcbor_tstr_put_term(zs, vendor_name);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__PRODUCT_NAME);
		zcbor_tstr_put_term(zs, product_name);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__HW_VARIANT);
		zcbor_tstr_put_term(zs, hw_variant);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__HW_REVISION);
		zcbor_tstr_put_term(zs, hw_revision);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__FW_VERSION);
		zcbor_tstr_put_term(zs, fw_version);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__SERIAL_NUMBER);
		zcbor_tstr_put_term(zs, serial_number);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		uint64_t imei;
		ret = ctr_lte_v2_get_imei(&imei);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_get_imei` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__IMEI);
		zcbor_uint64_put(zs, imei);

		uint64_t imsi;
		ret = ctr_lte_v2_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_get_imsi` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__IMSI);
		zcbor_uint64_put(zs, imsi);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_THERMOMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_THERMOMETER__TEMPERATURE);
		if (isnan(g_app_data.therm_temperature)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.therm_temperature * 100.f);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCELERATION_X);
		if (isnan(g_app_data.accel_acceleration_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_x * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCELERATION_Y);
		if (isnan(g_app_data.accel_acceleration_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_y * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCELERATION_Z);
		if (isnan(g_app_data.accel_acceleration_z)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_z * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ORIENTATION);
		if (g_app_data.accel_orientation == INT_MAX) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.accel_orientation);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

#if defined(CONFIG_APP_TAMPER)
	zcbor_uint32_put(zs, CODEC_KEY_E_TAMPER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_TAMPER__STATE);
		zcbor_uint32_put(zs, g_app_data.tamper.activated ? 1 : 0);

		if (g_app_data.tamper.event_count) {
			int64_t timestamp_abs = g_app_data.tamper.events[0].timestamp;

			zcbor_uint32_put(zs, CODEC_KEY_E_TAMPER__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				/* TSO absolute timestamp */
				zcbor_int64_put(zs, timestamp_abs);

				for (int i = 0; i < g_app_data.tamper.event_count; i++) {
					/* TSO offset timestamp */
					zcbor_int64_put(zs, g_app_data.tamper.events[i].timestamp -
								    timestamp_abs);
					zcbor_uint32_put(
						zs, g_app_data.tamper.events[i].activated ? 1 : 0);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		} else {
			zcbor_uint32_put(zs, CODEC_KEY_E_TAMPER__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_APP_TAMPER) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__LINE_VOLTAGE);
		if (isnan(g_app_data.backup.line_voltage)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.backup.line_voltage * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__BATT_VOLTAGE);
		if (isnan(g_app_data.backup.battery_voltage)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.backup.battery_voltage * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__STATE);
		zcbor_uint32_put(zs, g_app_data.backup.line_present ? 1 : 0);

		if (g_app_data.backup.event_count) {
			int64_t timestamp_abs = g_app_data.backup.events[0].timestamp;

			zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				/* TSO absolute timestamp */
				zcbor_int64_put(zs, timestamp_abs);

				for (int i = 0; i < g_app_data.backup.event_count; i++) {
					/* TSO offset timestamp */
					zcbor_int64_put(zs, g_app_data.backup.events[i].timestamp -
								    timestamp_abs);
					zcbor_uint32_put(
						zs, g_app_data.backup.events[i].connected ? 1 : 0);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		} else {
			zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_W1_THERM_COUNT; i++) {
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS__SERIAL_NUMBER);
			zcbor_uint64_put(zs, sensor->serial_number);

			zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.w1_therm.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int j = 0; j < sensor->measurement_count; j++) {
					put_sample_mul(zs, &sensor->measurements[j].temperature,
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	zcbor_uint32_put(zs, CODEC_KEY_E_BAROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_BAROMETER__PRESSURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_BAROMETER__PRESSURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.barometer.pressure.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.barometer.pressure.measurement_count;
				     i++) {
					put_sample(zs,
						   &g_app_data.barometer.pressure.measurements[i]);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__TEMPERATURE__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				if (g_app_data.hygro.event_count) {
					/* TSO absolute timestamp */
					int64_t timestamp_abs =
						g_app_data.hygro.events[0].timestamp;
					zcbor_int64_put(zs, timestamp_abs);

					for (int i = 0; i < g_app_data.hygro.event_count; i++) {
						/* TSO offset timestamp */
						zcbor_int64_put(
							zs, g_app_data.hygro.events[i].timestamp -
								    timestamp_abs);
						zcbor_uint32_put(zs,
								 g_app_data.hygro.events[i].type);
						zcbor_int32_put(zs,
								g_app_data.hygro.events[i].value *
									100.f);
					}
				}
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__TEMPERATURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.hygro.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.hygro.measurement_count; i++) {
					put_sample_mul(
						zs, &g_app_data.hygro.measurements[i].temperature,
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__HUMIDITY__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.hygro.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.hygro.measurement_count; i++) {
					put_sample_mul(zs,
						       &g_app_data.hygro.measurements[i].humidity,
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	zcbor_uint32_put(zs, CODEC_KEY_E_SOIL_SENSORS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
			struct app_data_soil_sensor_sensor *sensor =
				&g_app_data.soil_sensor.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_SOIL_SENSORS__SERIAL_NUMBER);
			zcbor_uint64_put(zs, sensor->serial_number);

			zcbor_uint32_put(zs, CODEC_KEY_E_SOIL_SENSORS__TEMPERATURE);
			{
				zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_uint32_put(
					zs, CODEC_KEY_E_SOIL_SENSORS__TEMPERATURE__MEASUREMENTS);
				{
					zcbor_list_start_encode(zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint64_put(zs, g_app_data.soil_sensor.timestamp);
					zcbor_uint32_put(zs, g_app_config.interval_aggreg);

					for (int j = 0; j < sensor->measurement_count; j++) {
						put_sample_mul(zs,
							       &sensor->measurements[j].temperature,
							       100.f);
					}
					zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
				zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_SOIL_SENSORS__MOISTURE);
			{
				zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_uint32_put(zs,
						 CODEC_KEY_E_SOIL_SENSORS__MOISTURE__MEASUREMENTS);
				{
					zcbor_list_start_encode(zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint64_put(zs, g_app_data.soil_sensor.timestamp);
					zcbor_uint32_put(zs, g_app_config.interval_aggreg);

					for (int j = 0; j < sensor->measurement_count; j++) {
						put_sample(zs, &sensor->measurements[j].moisture);
					}
					zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
				zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION__WIND_SPEED);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION__WIND_SPEED__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.meteo.wind_speed.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.meteo.wind_speed.measurement_count;
				     i++) {
					put_sample_mul(zs,
						       &g_app_data.meteo.wind_speed.measurements[i],
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION__WIND_DIRECTION);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs,
					 CODEC_KEY_E_WEATHER_STATION__WIND_DIRECTION__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.meteo.wind_direction.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0;
				     i < g_app_data.meteo.wind_direction.measurement_count; i++) {
					zcbor_int32_put(
						zs,
						g_app_data.meteo.wind_direction.measurements[i]);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION__RAINFALL);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_WEATHER_STATION__RAINFALL__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.meteo.rainfall.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.meteo.rainfall.measurement_count;
				     i++) {
					zcbor_int32_put(zs,
							g_app_data.meteo.rainfall.measurements[i] *
								100);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)
	zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__WIND_SPEED);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs,
				CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__WIND_SPEED__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs,
						&g_app_data.lambrecht.wind_speed_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__WIND_DIRECTION);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs,
				CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__WIND_DIRECTION__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(zs,
						       &g_app_data.lambrecht
								.wind_direction_measurements[i],
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs,
				CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__TEMPERATURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs,
						&g_app_data.lambrecht.temperature_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__HUMIDITY__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs, &g_app_data.lambrecht.humidity_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__DEW_POINT);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__DEW_POINT__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs, &g_app_data.lambrecht.dew_point_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__PRESSURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__PRESSURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs, &g_app_data.lambrecht.pressure_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__RAINFALL);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__RAINFALL__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(zs,
						       &g_app_data.lambrecht
								.rainfall_total_measurements[i],
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__RAINFALL_INTENSITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs,
				CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__RAINFALL_INTENSITY__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(zs,
						       &g_app_data.lambrecht
								.rainfall_intensity_measurements[i],
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__ILLUMINANCE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(
				zs,
				CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__ILLUMINANCE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.lambrecht.timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < g_app_data.lambrecht.measurement_count; i++) {
					put_sample_mul(
						zs,
						&g_app_data.lambrecht.illuminance_measurements[i],
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_LAMBRECHT_WEATHER_STATION__RAINFALL_TOTAL);
		zcbor_uint32_put(zs, g_app_data.lambrecht.rainfall_total_sum * 1000);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */

#if defined(CONFIG_CTR_BLE_TAG)
	zcbor_int32_put(zs, CODEC_KEY_E_BLE_TAGS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
			struct app_data_ble_tag_sensor *sensor = &g_app_data.ble_tag.sensor[i];

			if (ctr_ble_tag_is_addr_empty(sensor->addr)) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			{

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__ADDR);
				{
					char addr_str[BT_ADDR_SIZE * 2 + 1] = {0};
					ctr_buf2hex(sensor->addr, BT_ADDR_SIZE, addr_str,
						    BT_ADDR_SIZE * 2 + 1, false);
					zcbor_tstr_put_lit(zs, addr_str);
				}

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__RSSI);
				zcbor_int32_put(zs, sensor->rssi);

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__VOLTAGE);
				{
					zcbor_uint32_put(zs, sensor->voltage * 100.f);
				}

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__TEMPERATURE);
				{
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					zcbor_uint32_put(
						zs,
						CODEC_KEY_E_BLE_TAGS__TEMPERATURE__MEASUREMENTS);
					{
						zcbor_list_start_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

						zcbor_uint64_put(zs, g_app_data.ble_tag.timestamp);
						zcbor_uint32_put(zs, g_app_config.interval_aggreg);

						for (int j = 0; j < sensor->measurement_count;
						     j++) {
							put_sample_mul(zs,
								       &sensor->measurements[j]
										.temperature,
								       100.f);
						}
						zcbor_list_end_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}

				zcbor_int32_put(zs, CODEC_KEY_E_BLE_TAGS__HUMIDITY);
				{
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					zcbor_uint32_put(
						zs, CODEC_KEY_E_BLE_TAGS__HUMIDITY__MEASUREMENTS);
					{
						zcbor_list_start_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

						zcbor_uint64_put(zs, g_app_data.ble_tag.timestamp);
						zcbor_uint32_put(zs, g_app_config.interval_aggreg);

						for (int j = 0; j < sensor->measurement_count;
						     j++) {
							put_sample_mul(
								zs,
								&sensor->measurements[j].humidity,
								100.f);
						}
						zcbor_list_end_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
			}

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_CTR_BLE_TAG) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}

int app_cbor_encode(zcbor_state_t *zs)
{
	int ret;

	app_data_lock();

	ret = encode(zs);
	if (ret) {
		LOG_ERR("Call `encode` failed: %d", ret);
		app_data_unlock();
		return ret;
	}

	app_data_unlock();

	return 0;
}
