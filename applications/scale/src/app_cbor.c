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

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

int app_cbor_encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, MSG_KEY_FRAME);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		static uint32_t sequence;
		zcbor_uint32_put(zs, MSG_KEY_SEQUENCE);
		zcbor_uint32_put(zs, sequence++);

		uint8_t protocol = 4;
		zcbor_uint32_put(zs, MSG_KEY_PROTOCOL);
		zcbor_uint32_put(zs, protocol);

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

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		zcbor_uint32_put(zs, MSG_KEY_FW_NAME);
		zcbor_tstr_put_term(zs, "CHESTER Scale");

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
		zcbor_uint64_put(zs, k_uptime_get() / 1000);

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
		if (isnan(g_app_data.acceleration_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.acceleration_x * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Y);
		if (isnan(g_app_data.acceleration_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.acceleration_y * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Z);
		if (isnan(g_app_data.acceleration_z)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.acceleration_z * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ORIENTATION);
		if (g_app_data.orientation == INT_MAX) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.orientation);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	if (g_app_config.channel_a1_active || g_app_config.channel_a2_active ||
	    g_app_config.channel_b1_active || g_app_config.channel_b2_active) {
		zcbor_uint32_put(zs, MSG_KEY_WEIGHT_MEASUREMENTS);
		{
			zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint64_put(zs, g_app_data.weight_measurement_timestamp);

			for (int i = 0; i < g_app_data.weight_measurement_count; i++) {
				zcbor_uint32_put(
					zs, g_app_data.weight_measurements[i].timestamp_offset);

				if (!g_app_config.channel_a1_active ||
				    g_app_data.weight_measurements[i].a1_raw == INT32_MAX) {
					zcbor_nil_put(zs, NULL);
				} else {
					zcbor_int32_put(zs,
							g_app_data.weight_measurements[i].a1_raw);
				}

				if (!g_app_config.channel_a2_active ||
				    g_app_data.weight_measurements[i].a2_raw == INT32_MAX) {
					zcbor_nil_put(zs, NULL);
				} else {
					zcbor_int32_put(zs,
							g_app_data.weight_measurements[i].a2_raw);
				}

				if (!g_app_config.channel_b1_active ||
				    g_app_data.weight_measurements[i].b1_raw == INT32_MAX) {
					zcbor_nil_put(zs, NULL);
				} else {
					zcbor_int32_put(zs,
							g_app_data.weight_measurements[i].b1_raw);
				}

				if (!g_app_config.channel_b2_active ||
				    g_app_data.weight_measurements[i].b2_raw == INT32_MAX) {
					zcbor_nil_put(zs, NULL);
				} else {
					zcbor_int32_put(zs,
							g_app_data.weight_measurements[i].b2_raw);
				}
			}

			zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}
	}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	zcbor_uint32_put(zs, MSG_KEY_PEOPLE_MEASUREMENTS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint64_put(zs, g_app_data.people_measurement_timestamp);

		for (int i = 0; i < g_app_data.people_measurement_count; i++) {
			bool is_valid = g_app_data.people_measurements[i].is_valid;

			struct people_counter_measurement *measurement =
				&g_app_data.people_measurements[i].measurement;

			zcbor_uint32_put(zs, g_app_data.people_measurements[i].timestamp_offset);

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->motion_counter);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->pass_counter_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->pass_counter_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->stay_counter_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->stay_counter_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->total_time_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->total_time_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->consumed_energy);
			}
		}

		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}
