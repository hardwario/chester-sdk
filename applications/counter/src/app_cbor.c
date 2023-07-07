/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

int app_cbor_encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, MSG_KEY_FRAME);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		uint8_t protocol = 1;
		zcbor_uint32_put(zs, MSG_KEY_PROTOCOL);
		zcbor_uint32_put(zs, protocol);

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
		zcbor_uint32_put(zs, timestamp);

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

	zcbor_uint32_put(zs, MSG_KEY_STATE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_UPTIME);
		zcbor_uint32_put(zs, k_uptime_get() / 1000);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_BATTERY);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_REST);
		if (isnan(g_app_data.batt_voltage_rest)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.batt_voltage_rest * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_LOAD);
		if (isnan(g_app_data.batt_voltage_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.batt_voltage_load * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_CURRENT_LOAD);
		if (isnan(g_app_data.batt_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.batt_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

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
		zcbor_uint32_put(zs, imei);

		uint64_t imsi;
		ret = ctr_lte_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_IMSI);
		zcbor_uint32_put(zs, imsi);

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

		zcbor_uint32_put(zs, MSG_KEY_ACCEL_X);
		if (isnan(g_app_data.accel_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_x * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCEL_Y);
		if (isnan(g_app_data.accel_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_y * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCEL_Z);
		if (isnan(g_app_data.accel_z)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.accel_z * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ORIENTATION);
		if (g_app_data.accel_orientation == INT_MAX) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.accel_orientation);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_COUNTER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)
		app_data_lock();
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
		uint64_t counter_ch1_total = g_app_data.counter_ch1_total;
		uint64_t counter_ch1_delta = g_app_data.counter_ch1_delta;
		uint64_t counter_ch2_total = g_app_data.counter_ch2_total;
		uint64_t counter_ch2_delta = g_app_data.counter_ch2_delta;
		uint64_t counter_ch3_total = g_app_data.counter_ch3_total;
		uint64_t counter_ch3_delta = g_app_data.counter_ch3_delta;
		uint64_t counter_ch4_total = g_app_data.counter_ch4_total;
		uint64_t counter_ch4_delta = g_app_data.counter_ch4_delta;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
		uint64_t counter_ch5_total = g_app_data.counter_ch5_total;
		uint64_t counter_ch5_delta = g_app_data.counter_ch5_delta;
		uint64_t counter_ch6_total = g_app_data.counter_ch6_total;
		uint64_t counter_ch6_delta = g_app_data.counter_ch6_delta;
		uint64_t counter_ch7_total = g_app_data.counter_ch7_total;
		uint64_t counter_ch7_delta = g_app_data.counter_ch7_delta;
		uint64_t counter_ch8_total = g_app_data.counter_ch8_total;
		uint64_t counter_ch8_delta = g_app_data.counter_ch8_delta;
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)
		app_data_unlock();
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_1_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch1_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_1_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch1_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_2_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch2_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_2_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch2_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_3_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch3_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_3_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch3_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_4_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch4_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_4_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		zcbor_uint32_put(zs, counter_ch4_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_5_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch5_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_5_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch5_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_6_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch6_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_6_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch6_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_7_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch7_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_7_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch7_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_8_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch8_total);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		zcbor_uint32_put(zs, MSG_KEY_CHANNEL_8_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		zcbor_uint32_put(zs, counter_ch8_delta);
#else
		zcbor_nil_put(zs, NULL);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

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

#if defined(CONFIG_SHIELD_CTR_Z)
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
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	
	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}
	
	return 0;
}
