/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* ### Preserved code "functions" (begin) */
#include "drivers/drv_ld81.h"

#include <chester/ctr_power.h>

__unused static void put_sample_mul(zcbor_state_t *zs, struct ctr_data_aggreg *sample, float mul)
{
	if (isnan(sample->min)) {
		zcbor_nil_put(zs, NULL);
	} else {
		int32_t v = sample->min * mul;
		zcbor_int32_put(zs, v);
	}

	if (isnan(sample->max)) {
		zcbor_nil_put(zs, NULL);
	} else {
		int32_t v = sample->max * mul;
		zcbor_int32_put(zs, v);
	}

	if (isnan(sample->avg)) {
		zcbor_nil_put(zs, NULL);
	} else {
		int32_t v = sample->avg * mul;
		zcbor_int32_put(zs, v);
	}

	if (isnan(sample->mdn)) {
		zcbor_nil_put(zs, NULL);
	} else {
		int32_t v = sample->mdn * mul;
		zcbor_int32_put(zs, v);
	}
}

__unused static void put_sample(zcbor_state_t *zs, struct ctr_data_aggreg *sample)
{
	put_sample_mul(zs, sample, 1.f);
}
/* ^^^ Preserved code "functions" (end) */

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

static int encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	/* ### Preserved code "message" (begin) */

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

	/* ^^^ Preserved code "message" (end) */

	/* ### Preserved code "system" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__UPTIME);
		zcbor_uint64_put(zs, k_uptime_get() / 1000);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "system" (end) */

	/* ### Preserved code "power" (begin) */

#if defined(FEATURE_SUBSYSTEM_POWER)
	ret = ctr_power_cbor_encode(zs);
	if (ret) {
		LOG_ERR("Call `ctr_power_cbor_encode` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_POWER) */

	/* ^^^ Preserved code "power" (end) */

	/* ### Preserved code "network" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			static struct ctr_lte_v2_conn_param conn_param;
			ret = ctr_lte_v2_get_conn_param(&conn_param);
			if (ret) {
				LOG_ERR("Call `ctr_lte_v2_state_get_conn_param` failed: %d", ret);
				return ret;
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__EEST);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.eest);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__ECL);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.ecl);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__RSRP);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.rsrp);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__RSRQ);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.rsrq);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__SNR);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.snr);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__PLMN);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.plmn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__CID);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.cid);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__BAND);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.band);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_NETWORK__PARAMETER__EARFCN);
			if (conn_param.valid) {
				zcbor_int32_put(zs, conn_param.earfcn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "network" (end) */

	/* ### Preserved code "thermometer" (begin) */

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

	/* ^^^ Preserved code "thermometer" (end) */

	/* ### Preserved code "accelerometer" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCEL_X);
		if (isnan(g_app_data.accel_acceleration_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_x * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCEL_Y);
		if (isnan(g_app_data.accel_acceleration_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_y * 1000.f);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_ACCELEROMETER__ACCEL_Z);
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

	/* ^^^ Preserved code "accelerometer" (end) */

	/* ### Preserved code "trigger" (begin) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		{
			for (int ch_index = 0; ch_index < APP_DATA_NUM_CHANNELS; ch_index++) {

				struct app_data_trigger *trigger = g_app_data.trigger[ch_index];
				if (trigger) {
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER__CHANNEL);
					zcbor_uint32_put(zs, ch_index + 1);

					zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER__STATE);
					zcbor_uint32_put(zs, trigger->trigger_active ? 1 : 0);

					if (trigger->event_count) {

						int64_t timestamp_abs =
							trigger->events[0].timestamp;

						zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER__EVENTS);
						{
							zcbor_list_start_encode(
								zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

							/* TSO absolute timestamp */
							zcbor_int64_put(zs, timestamp_abs);

							for (int i = 0; i < trigger->event_count;
							     i++) {
								/* TSO offset timestamp */
								zcbor_int64_put(
									zs,
									trigger->events[i]
											.timestamp -
										timestamp_abs);
								zcbor_uint32_put(
									zs,
									trigger->events[i].is_active
										? 1
										: 0);

								LOG_WRN("Adding trigger event: %d",
									trigger->events[i]
										.is_active);
							}

							zcbor_list_end_encode(
								zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
						}
					} else {
						zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER__EVENTS);
						{
							zcbor_list_start_encode(
								zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
							zcbor_list_end_encode(
								zs,
								ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
						}
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
			}
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "trigger" (end) */

	/* ### Preserved code "counter" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_COUNTER);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		{
			for (int ch_index = 0; ch_index < APP_DATA_NUM_CHANNELS; ch_index++) {
				struct app_data_counter *counter = g_app_data.counter[ch_index];
				if (counter) {
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint32_put(zs, CODEC_KEY_E_TRIGGER__CHANNEL);
					zcbor_uint32_put(zs, ch_index + 1);

					zcbor_uint32_put(zs, CODEC_KEY_E_COUNTER__VALUE);
					zcbor_uint64_put(zs, counter->value);

					zcbor_uint32_put(zs, CODEC_KEY_E_COUNTER__DELTA);
					zcbor_uint64_put(zs, counter->delta);

					zcbor_uint32_put(zs, CODEC_KEY_E_COUNTER__MEASUREMENTS);
					{
						zcbor_list_start_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

						zcbor_uint64_put(zs, counter->timestamp);
						zcbor_uint32_put(
							zs, g_app_config.counter_interval_aggreg);

						for (int i = 0; i < counter->measurement_count;
						     i++) {
							zcbor_uint64_put(
								zs, counter->measurements[i].value);
							zcbor_uint64_put(
								zs, counter->measurements[i].delta);
						}

						zcbor_list_end_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
			}
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "counter" (end) */

	/* ### Preserved code "voltage" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_VOLTAGE);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		{
			for (int ch_index = 0; ch_index < APP_DATA_NUM_CHANNELS; ch_index++) {
				struct app_data_analog *voltage = g_app_data.voltage[ch_index];
				if (voltage) {

					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint32_put(zs, CODEC_KEY_E_VOLTAGE__CHANNEL);
					zcbor_uint32_put(zs, ch_index + 1);

					zcbor_uint32_put(zs, CODEC_KEY_E_VOLTAGE__MEASUREMENTS_DIV);
					{
						zcbor_list_start_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

						zcbor_uint64_put(zs, voltage->timestamp);
						zcbor_uint32_put(
							zs, g_app_config.analog_interval_aggreg);

						for (int i = 0; i < voltage->measurement_count;
						     i++) {
							put_sample_mul(zs,
								       &voltage->measurements[i],
								       100.f);
						}

						zcbor_list_end_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
			}
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "voltage" (end) */

	/* ### Preserved code "current" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_CURRENT);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		{
			for (int ch_index = 0; ch_index < APP_DATA_NUM_CHANNELS; ch_index++) {
				struct app_data_analog *current = g_app_data.current[ch_index];
				if (current) {
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

					zcbor_uint32_put(zs, CODEC_KEY_E_CURRENT__CHANNEL);
					zcbor_uint32_put(zs, ch_index + 1);

					zcbor_uint32_put(zs, CODEC_KEY_E_CURRENT__MEASUREMENTS_DIV);
					{
						zcbor_list_start_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

						zcbor_uint64_put(zs, current->timestamp);
						zcbor_uint32_put(
							zs, g_app_config.analog_interval_aggreg);

						for (int i = 0; i < current->measurement_count;
						     i++) {
							put_sample_mul(zs,
								       &current->measurements[i],
								       100.f);
						}

						zcbor_list_end_encode(
							zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					}
					zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				}
			}
		}
		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

	/* ^^^ Preserved code "current" (end) */

	/* ### Preserved code "hygrometer" (begin) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		/* ### Preserved code "temperature" (begin) */

		zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__TEMPERATURE__MEASUREMENTS_DIV);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.hygro.timestamp);
				zcbor_uint32_put(zs, g_app_config.hygro_interval_aggreg);

				for (int i = 0; i < g_app_data.hygro.measurement_count; i++) {
					put_sample_mul(
						zs, &g_app_data.hygro.measurements[i].temperature,
						100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		/* ^^^ Preserved code "temperature" (end) */

		/* ### Preserved code "humidity" (begin) */

		zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_HYGROMETER__HUMIDITY__MEASUREMENTS_DIV);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.hygro.timestamp);
				zcbor_uint32_put(zs, g_app_config.hygro_interval_aggreg);

				for (int i = 0; i < g_app_data.hygro.measurement_count; i++) {
					put_sample_mul(zs,
						       &g_app_data.hygro.measurements[i].humidity,
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		/* ^^^ Preserved code "humidity" (end) */

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

	/* ^^^ Preserved code "hygrometer" (end) */

	/* ### Preserved code "w1_thermometers" (begin) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < g_app_data.w1_therm.sensor_count; i++) {
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS__SERIAL_NUMBER);
			zcbor_uint64_put(zs, sensor->serial_number);

			zcbor_uint32_put(zs, CODEC_KEY_E_W1_THERMOMETERS__MEASUREMENTS_DIV);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.w1_therm.timestamp);
				zcbor_uint32_put(zs, g_app_config.w1_therm_interval_aggreg);

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

	/* ^^^ Preserved code "w1_thermometers" (end) */

	/* ### Preserved code "soil_sensors" (begin) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	zcbor_uint32_put(zs, CODEC_KEY_E_SOIL_SENSORS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < g_app_data.soil_sensor.sensor_count; i++) {
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
					zcbor_uint32_put(zs, g_app_config.w1_therm_interval_aggreg);

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
					zcbor_uint32_put(zs, g_app_config.w1_therm_interval_aggreg);

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

	/* ^^^ Preserved code "soil_sensors" (end) */

	/* ### Preserved code "ble_tags" (begin) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
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
				if (sensor->rssi < 0) {
					zcbor_int32_put(zs, sensor->rssi);
				} else {
					zcbor_nil_put(zs, NULL);
				}

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__VOLTAGE);
				if (!isnan(sensor->voltage)) {
					zcbor_uint32_put(zs, sensor->voltage * 100.f);
				} else {
					zcbor_nil_put(zs, NULL);
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
						zcbor_uint32_put(
							zs, g_app_config.analog_interval_aggreg);

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
						zcbor_uint32_put(
							zs, g_app_config.analog_interval_aggreg);

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
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

	/* ^^^ Preserved code "ble_tags" (end) */

	/* ### Preserved code "sensors" (begin) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	/* Pluggable sensors (LD-81 etc.) — one map per configured slot. */
	zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	for (int slot = 0; slot < APP_SENSORS_MAX; slot++) {
		struct app_data_sensor *s = &g_app_data.sensors[slot];
		if (!s->configured) {
			continue;
		}

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__INDEX);
		zcbor_uint32_put(zs, slot + 1);
		zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__TYPE);
		zcbor_uint32_put(zs, s->type_id);
		zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__CHANNEL_POWER);
		zcbor_uint32_put(zs, s->ch_power);
		zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__CHANNEL_MEASURE);
		zcbor_uint32_put(zs, s->ch_measure);

		zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__DATA);
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		switch (s->type_id) {
		case APP_SENSOR_TYPE_LD81: {
			/* One snapshot, so the millivolts and the flood state in this
			 * payload always come from the same sample. */
			struct app_data_ld81 d;
			bool valid = ld81_get_data(slot, &d) == 0 && d.valid;

			zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__DATA__SENSOR_VOLTAGE);
			if (!valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_int32_put(zs, d.voltage_mv);
			}
			zcbor_uint32_put(zs, CODEC_KEY_E_SENSORS__DATA__FLOODED);
			if (!valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_bool_put(zs, d.flooded);
			}
			break;
		}
		default:
			break;
		}
		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

	/* ^^^ Preserved code "sensors" (end) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}

int decode(zcbor_state_t *zs, struct app_cbor_received *received)
{
	memset(received, 0, sizeof(struct app_cbor_received));

	if (!zcbor_map_start_decode(zs)) {
		return -EBADMSG;
	}

	uint32_t key;
	bool ok;

	/* ### Preserved code "while" (begin) */
	while (1) {
		if (!zcbor_uint32_decode(zs, &key)) {
			break;
		}

		switch (key) {

		/* Filled in by Project Generator */
		case CODEC_KEY_D_OUTPUT_1_STATE:
			ok = zcbor_int32_decode(zs, &received->output_1_state);
			received->has_output_1_state = true;
			break;
		case CODEC_KEY_D_OUTPUT_2_STATE:
			ok = zcbor_int32_decode(zs, &received->output_2_state);
			received->has_output_2_state = true;
			break;
		case CODEC_KEY_D_OUTPUT_3_STATE:
			ok = zcbor_int32_decode(zs, &received->output_3_state);
			received->has_output_3_state = true;
			break;
		case CODEC_KEY_D_OUTPUT_4_STATE:
			ok = zcbor_int32_decode(zs, &received->output_4_state);
			received->has_output_4_state = true;
			break;
		}
		if (!ok) {
			LOG_ERR("Encoding failed: %d", zcbor_peek_error(zs));
			return -EBADMSG;
		}
	}
	/* ^^^ Preserved code "while" (end) */

	if (!zcbor_map_end_decode(zs)) {
		return -EBADMSG;
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

int app_cbor_decode(zcbor_state_t *zs, struct app_cbor_received *received)
{
	int ret;
	ret = decode(zs, received);
	if (ret) {
		LOG_ERR("Call `decode` failed: %d", ret);
		return ret;
	}

	return 0;
}