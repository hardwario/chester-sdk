/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_codec.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
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

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

__unused static void put_sample(zcbor_state_t *zs, struct app_data_aggreg *sample)
{
	if (isnan(sample->min)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->min);
	}

	if (isnan(sample->max)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->max);
	}

	if (isnan(sample->avg)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->avg);
	}

	if (isnan(sample->mdn)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, sample->mdn);
	}
}

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

static int encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__VERSION);
		zcbor_uint32_put(zs, 2);

		static uint32_t sequence;
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

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
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

			zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__STATE__EVENTS);
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
			zcbor_uint32_put(zs, CODEC_KEY_E_BACKUP__STATE__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

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

#if defined(CONFIG_SHIELD_CTR_S1)
	zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR);
	{
		struct app_data_iaq_sensors *sensors = &g_app_data.iaq.sensors;
		struct app_data_iaq_button *button = &g_app_data.iaq.button;

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__TEMPERATURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample_mul(zs, &sensors->measurements[i].temperature,
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__HUMIDITY__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);
				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample_mul(zs, &sensors->measurements[i].humidity,
						       100.f);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__ILLUMINANCE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__ILLUMINANCE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample(zs, &sensors->measurements[i].illuminance);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__ALTITUDE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__ALTITUDE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample(zs, &sensors->measurements[i].altitude);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__PRESSURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__PRESSURE__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample(zs, &sensors->measurements[i].pressure);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__CO2_CONC);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__CO2_CONC__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					put_sample(zs, &sensors->measurements[i].co2_conc);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__MOTION_COUNT);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__MOTION_COUNT__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, sensors->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < sensors->measurement_count; i++) {
					zcbor_int32_put(zs, sensors->measurements[i].motion_count);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__PRESS_COUNT);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_IAQ_SENSOR__PRESS_COUNT__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, button->timestamp);
				zcbor_uint32_put(zs, g_app_config.interval_aggreg);

				for (int i = 0; i < button->measurement_count; i++) {
					zcbor_int32_put(zs, button->measurements[i].press_count);
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
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
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
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
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
	zcbor_uint32_put(zs, CODEC_KEY_E_RTD_THERMOMETER);
	{
		const int channel_lookup[] = {
#if defined(CONFIG_SHIELD_CTR_RTD_A)
			1,
			2,
#endif
#if defined(CONFIG_SHIELD_CTR_RTD_B)
			3,
			4,
#endif
		};

		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
			struct app_data_rtd_therm_sensor *sensor = &g_app_data.rtd_therm.sensor[i];

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_RTD_THERMOMETER__CHANNEL);
			zcbor_uint32_put(zs, channel_lookup[i]);

			zcbor_uint32_put(zs, CODEC_KEY_E_RTD_THERMOMETER__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.rtd_therm.timestamp);
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
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_A) || defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_B)
	zcbor_uint32_put(zs, CODEC_KEY_E_TC_THERMOMETER);
	{
		const int channel_lookup[] = {
#if defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_A)
			1,
			2,
#endif
#if defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_B)
			3,
			4,
#endif
		};

		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {
			struct app_data_tc_therm_sensor *sensor = &g_app_data.tc_therm.sensor[i];

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, CODEC_KEY_E_TC_THERMOMETER__CHANNEL);
			zcbor_uint32_put(zs, channel_lookup[i]);

			zcbor_uint32_put(zs, CODEC_KEY_E_TC_THERMOMETER__MEASUREMENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				zcbor_uint64_put(zs, g_app_data.tc_therm.timestamp);
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
#endif /* defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_A) || defined(FEATURE_SUBSYSTEM_THERMOCOUPLE_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
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
#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

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
				if (sensor->rssi != INT_MAX) {
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
