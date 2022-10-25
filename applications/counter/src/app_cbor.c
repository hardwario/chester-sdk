#include "app_cbor.h"
#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

#include <tinycbor/cbor.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

int app_cbor_encode(CborEncoder *enc)
{
	int ret;

	CborError err = 0;

	CborEncoder map;
	err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

	err |= cbor_encode_uint(&map, MSG_KEY_FRAME);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		uint8_t protocol = 1;
		err |= cbor_encode_uint(&map, MSG_KEY_PROTOCOL);
		err |= cbor_encode_uint(&map, protocol);

		static uint32_t sequence;
		err |= cbor_encode_uint(&map, MSG_KEY_SEQUENCE);
		err |= cbor_encode_uint(&map, sequence++);

		uint64_t timestamp;
		ret = ctr_rtc_get_ts(&timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}

		err |= cbor_encode_uint(&map, MSG_KEY_TIMESTAMP);
		err |= cbor_encode_uint(&map, timestamp);

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_ATTRIBUTE);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		err |= cbor_encode_uint(&map, MSG_KEY_VENDOR_NAME);
		err |= cbor_encode_text_stringz(&map, vendor_name);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		err |= cbor_encode_uint(&map, MSG_KEY_PRODUCT_NAME);
		err |= cbor_encode_text_stringz(&map, product_name);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		err |= cbor_encode_uint(&map, MSG_KEY_HW_VARIANT);
		err |= cbor_encode_text_stringz(&map, hw_variant);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		err |= cbor_encode_uint(&map, MSG_KEY_HW_REVISION);
		err |= cbor_encode_text_stringz(&map, hw_revision);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		err |= cbor_encode_uint(&map, MSG_KEY_FW_VERSION);
		err |= cbor_encode_text_stringz(&map, fw_version);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		err |= cbor_encode_uint(&map, MSG_KEY_SERIAL_NUMBER);
		err |= cbor_encode_text_stringz(&map, serial_number);

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_STATE);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_UPTIME);
		err |= cbor_encode_uint(&map, k_uptime_get() / 1000);

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_BATTERY);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE_REST);
		if (isnan(g_app_data.batt_voltage_rest)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.batt_voltage_rest * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE_LOAD);
		if (isnan(g_app_data.batt_voltage_load)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.batt_voltage_load * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_CURRENT_LOAD);
		if (isnan(g_app_data.batt_current_load)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.batt_current_load);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_NETWORK);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		uint64_t imei;
		ret = ctr_lte_get_imei(&imei);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
			return ret;
		}

		err |= cbor_encode_uint(&map, MSG_KEY_IMEI);
		err |= cbor_encode_uint(&map, imei);

		uint64_t imsi;
		ret = ctr_lte_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
			return ret;
		}

		err |= cbor_encode_uint(&map, MSG_KEY_IMSI);
		err |= cbor_encode_uint(&map, imsi);

		err |= cbor_encode_uint(&map, MSG_KEY_PARAMETER);
		{
			CborEncoder map;
			err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

			k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);

			err |= cbor_encode_uint(&map, MSG_KEY_EEST);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.eest);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_ECL);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.ecl);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_RSRP);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.rsrp);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_RSRQ);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.rsrq);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_SNR);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.snr);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_PLMN);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.plmn);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_CID);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.cid);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_BAND);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.band);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_EARFCN);
			if (g_app_data_lte_eval_valid) {
				err |= cbor_encode_int(&map, g_app_data_lte_eval.earfcn);
			} else {
				err |= cbor_encode_null(&map);
			}

			g_app_data_lte_eval_valid = false;

			k_mutex_unlock(&g_app_data_lte_eval_mut);

			err |= cbor_encoder_close_container(enc, &map);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_THERMOMETER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_TEMPERATURE);
		if (isnan(g_app_data.therm_temperature)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.therm_temperature * 100.f);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_ACCELEROMETER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_X);
		if (isnan(g_app_data.accel_x)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.accel_x * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_Y);
		if (isnan(g_app_data.accel_y)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.accel_y * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_Z);
		if (isnan(g_app_data.accel_z)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.accel_z * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ORIENTATION);
		if (g_app_data.accel_orientation == INT_MAX) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.accel_orientation);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_COUNTER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)
		k_mutex_lock(&g_app_data_lock, K_FOREVER);
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
		k_mutex_unlock(&g_app_data_lock);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_1_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch1_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_1_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch1_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_2_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch2_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_2_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch2_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_3_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch3_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_3_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch3_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_4_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch4_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_4_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_A)
		err |= cbor_encode_uint(&map, counter_ch4_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_5_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch5_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_5_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch5_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_6_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch6_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_6_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch6_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_7_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch7_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_7_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch7_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_8_TOTAL);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch8_total);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encode_uint(&map, MSG_KEY_CHANNEL_8_DELTA);
#if defined(CONFIG_SHIELD_CTR_X0_B)
		err |= cbor_encode_uint(&map, counter_ch8_delta);
#else
		err |= cbor_encode_null(&map);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encoder_close_container(enc, &map);

	return err ? -EFAULT : 0;
}
