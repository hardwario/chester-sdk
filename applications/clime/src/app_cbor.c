/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_rtc.h>
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

	zcbor_uint32_put(zs, MSG_KEY_MESSAGE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_VERSION);
		zcbor_uint32_put(zs, 1);

		static uint32_t sequence;
		zcbor_uint32_put(zs, MSG_KEY_SEQUENCE);
		zcbor_uint32_put(zs, sequence++);

		uint64_t timestamp;
		ret = ctr_rtc_get_ts(&timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_TIMESTAMP);
		zcbor_uint64_put(zs, timestamp);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_ATTRIBUTE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		zcbor_uint32_put(zs, MSG_KEY_VENDOR_NAME);
		zcbor_tstr_put_term(zs, vendor_name);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		zcbor_uint32_put(zs, MSG_KEY_PRODUCT_NAME);
		zcbor_tstr_put_term(zs, product_name);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		zcbor_uint32_put(zs, MSG_KEY_HW_VARIANT);
		zcbor_tstr_put_term(zs, hw_variant);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		zcbor_uint32_put(zs, MSG_KEY_HW_REVISION);
		zcbor_tstr_put_term(zs, hw_revision);

		char *fw_name;
		ctr_info_get_fw_name(&fw_name);

		zcbor_uint32_put(zs, MSG_KEY_FW_NAME);
		zcbor_tstr_put_term(zs, fw_name);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		zcbor_uint32_put(zs, MSG_KEY_FW_VERSION);
		zcbor_tstr_put_term(zs, fw_version);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		zcbor_uint32_put(zs, MSG_KEY_SERIAL_NUMBER);
		zcbor_tstr_put_term(zs, serial_number);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_SYSTEM);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_UPTIME);
		zcbor_uint64_put(zs, k_uptime_get() / 1000);

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_REST);
		if (isnan(g_app_data.system_voltage_rest)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_rest);
		}

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_LOAD);
		if (isnan(g_app_data.system_voltage_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_load);
		}

		zcbor_uint32_put(zs, MSG_KEY_CURRENT_LOAD);
		if (isnan(g_app_data.system_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

#if defined(CONFIG_APP_TAMPER)
	zcbor_uint32_put(zs, MSG_KEY_TAMPER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TAMPER_STATE);
		zcbor_uint32_put(zs, g_app_data.tamper.activated ? 1 : 0);

		if (g_app_data.tamper.event_count) {
			int64_t timestamp_abs = g_app_data.tamper.events[0].timestamp;

			zcbor_uint32_put(zs, MSG_KEY_TAMPER_EVENTS);
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
			zcbor_uint32_put(zs, MSG_KEY_TAMPER_EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_APP_TAMPER) */

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
	zcbor_uint32_put(zs, MSG_KEY_BACKUP);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_LINE_VOLTAGE);
		if (isnan(g_app_data.backup.line_voltage)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.backup.line_voltage * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_BATT_VOLTAGE);
		if (isnan(g_app_data.backup.battery_voltage)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.backup.battery_voltage * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_BACKUP_STATE);
		zcbor_uint32_put(zs, g_app_data.backup.line_present ? 1 : 0);

		if (g_app_data.backup.event_count) {
			int64_t timestamp_abs = g_app_data.backup.events[0].timestamp;

			zcbor_uint32_put(zs, MSG_KEY_BACKUP_EVENTS);
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
			zcbor_uint32_put(zs, MSG_KEY_BACKUP_EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

	zcbor_uint32_put(zs, MSG_KEY_NETWORK);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		uint64_t imei;
		ret = ctr_lte_get_imei(&imei);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_IMEI);
		zcbor_uint64_put(zs, imei);

		uint64_t imsi;
		ret = ctr_lte_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_IMSI);
		zcbor_uint64_put(zs, imsi);

		zcbor_uint32_put(zs, MSG_KEY_PARAMETER);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);

			zcbor_uint32_put(zs, MSG_KEY_EEST);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.eest);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_ECL);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.ecl);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_RSRP);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.rsrp);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_RSRQ);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.rsrq);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_SNR);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.snr);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_PLMN);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.plmn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_CID);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.cid);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_BAND);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.band);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_EARFCN);
			if (g_app_data_lte_eval_valid) {
				zcbor_int32_put(zs, g_app_data_lte_eval.earfcn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			g_app_data_lte_eval_valid = false;

			k_mutex_unlock(&g_app_data_lte_eval_mut);

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_THERMOMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		if (isnan(g_app_data.therm_temperature)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.therm_temperature * 100.f);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_ACCELEROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_X);
		if (isnan(g_app_data.accel_acceleration_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_x * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Y);
		if (isnan(g_app_data.accel_acceleration_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_y * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Z);
		if (isnan(g_app_data.accel_acceleration_z)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_acceleration_z * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ORIENTATION);
		if (g_app_data.accel_orientation == INT_MAX) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.accel_orientation);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

#if defined(CONFIG_SHIELD_CTR_S1)
	zcbor_uint32_put(zs, MSG_KEY_IAQ_SENSOR);
	{
		struct app_data_iaq_sensors *sensors = &g_app_data.iaq.sensors;
		struct app_data_iaq_button *button = &g_app_data.iaq.button;

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

		zcbor_uint32_put(zs, MSG_KEY_HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

		zcbor_uint32_put(zs, MSG_KEY_ILLUMINANCE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS);
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

		zcbor_uint32_put(zs, MSG_KEY_ALTITUDE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS);
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

		zcbor_uint32_put(zs, MSG_KEY_PRESSURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS);
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

		zcbor_uint32_put(zs, MSG_KEY_CO2_CONC);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS);
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

		zcbor_uint32_put(zs, MSG_KEY_MOTION_COUNT);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_VAL);
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

		zcbor_uint32_put(zs, MSG_KEY_PRESS_COUNT);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_VAL);
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
	zcbor_uint32_put(zs, MSG_KEY_HYGROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_HYGRO_EVENTS);
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

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

		zcbor_uint32_put(zs, MSG_KEY_HUMIDITY);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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
	zcbor_uint32_put(zs, MSG_KEY_W1_THERMOMETERS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_W1_THERM_COUNT; i++) {
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_SERIAL_NUMBER);
			zcbor_uint64_put(zs, sensor->serial_number);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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
	zcbor_uint32_put(zs, MSG_KEY_RTD_THERMOMETER);
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

			zcbor_uint32_put(zs, MSG_KEY_CHANNEL);
			zcbor_uint32_put(zs, channel_lookup[i]);

			zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
	zcbor_uint32_put(zs, MSG_KEY_SOIL_SENSORS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
			struct app_data_soil_sensor_sensor *sensor =
				&g_app_data.soil_sensor.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_SERIAL_NUMBER);
			zcbor_uint64_put(zs, sensor->serial_number);

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
			{
				zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

			zcbor_uint32_put(zs, MSG_KEY_MOISTURE);
			{
				zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
				zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS);
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

	zcbor_int32_put(zs, MSG_KEY_BLE_TAGS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
			struct app_data_ble_tag_sensor *sensor = &g_app_data.ble_tag.sensor[i];

			if (ctr_ble_tag_is_addr_empty(sensor->addr)) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			{

				zcbor_uint32_put(zs, MSG_KEY_ADDR);
				{
					char addr_str[BT_ADDR_SIZE * 2 + 1] = {0};
					ctr_buf2hex(sensor->addr, BT_ADDR_SIZE, addr_str,
						    BT_ADDR_SIZE * 2 + 1, false);
					zcbor_tstr_put_lit(zs, addr_str);
				}

				zcbor_uint32_put(zs, MSG_KEY_RSSI);
				zcbor_int32_put(zs, sensor->rssi);

				zcbor_uint32_put(zs, MSG_KEY_VOLTAGE);
				{
					zcbor_uint32_put(zs, sensor->voltage * 100.f);
				}

				zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
				{
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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

				zcbor_int32_put(zs, MSG_KEY_HUMIDITY);
				{
					zcbor_map_start_encode(zs,
							       ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
					zcbor_uint32_put(zs, MSG_KEY_MEASUREMENTS_DIV);
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
