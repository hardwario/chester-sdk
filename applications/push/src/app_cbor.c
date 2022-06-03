#include "app_cbor.h"
#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <ctr_info.h>
#include <ctr_lte.h>
#include <ctr_rtc.h>

/* Zephyr includes */
#include <logging/log.h>
#include <tinycbor/cbor.h>
#include <zephyr.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

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
		if (g_app_data.errors.batt_voltage_rest) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.batt_voltage_rest * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE_LOAD);
		if (g_app_data.errors.batt_voltage_load) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.batt_voltage_load * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_CURRENT_LOAD);
		if (g_app_data.errors.batt_current_load) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.batt_current_load);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_BACKUP);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE);
		if (g_app_data.errors.bckp_voltage) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.bckp_voltage * 1000.f);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_LINE);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_PRESENT);
		if (g_app_data.errors.line_present) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_boolean(&map, g_app_data.states.line_present);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE);
		if (g_app_data.errors.line_voltage) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.line_voltage * 1000.f);
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
		if (g_app_data.errors.int_temperature) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.states.int_temperature * 100.f);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_ACCELEROMETER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_ACCELERATION_X);
		if (g_app_data.errors.acceleration_x) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.states.acceleration_x * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCELERATION_Y);
		if (g_app_data.errors.acceleration_y) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.states.acceleration_y * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCELERATION_Z);
		if (g_app_data.errors.acceleration_z) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, g_app_data.states.acceleration_z * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ORIENTATION);
		if (g_app_data.errors.orientation) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, g_app_data.states.orientation);
		}

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_BUTTON);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_X_CLICK_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_x_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_1_CLICK_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_1_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_2_CLICK_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_2_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_3_CLICK_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_3_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_4_CLICK_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_4_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_X_HOLD_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_x_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_1_HOLD_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_1_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_2_HOLD_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_2_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_3_HOLD_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_3_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_4_HOLD_EVENT);
		err |= cbor_encode_boolean(&map, atomic_get(&g_app_data.sources.button_4_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_X_CLICK_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_x_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_1_CLICK_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_1_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_2_CLICK_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_2_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_3_CLICK_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_3_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_4_CLICK_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_4_click));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_X_HOLD_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_x_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_1_HOLD_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_1_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_2_HOLD_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_2_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_3_HOLD_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_3_hold));

		err |= cbor_encode_uint(&map, MSG_KEY_BUTTON_4_HOLD_COUNT);
		err |= cbor_encode_uint(&map, atomic_get(&g_app_data.events.button_4_hold));

		err |= cbor_encoder_close_container(enc, &map);
	}

	err |= cbor_encoder_close_container(enc, &map);

	atomic_set(&g_app_data.sources.button_x_click, false);
	atomic_set(&g_app_data.sources.button_x_hold, false);
	atomic_set(&g_app_data.sources.button_1_click, false);
	atomic_set(&g_app_data.sources.button_1_hold, false);
	atomic_set(&g_app_data.sources.button_2_click, false);
	atomic_set(&g_app_data.sources.button_2_hold, false);
	atomic_set(&g_app_data.sources.button_3_click, false);
	atomic_set(&g_app_data.sources.button_3_hold, false);
	atomic_set(&g_app_data.sources.button_4_click, false);
	atomic_set(&g_app_data.sources.button_4_hold, false);

	return err ? -ENOSPC : 0;
}
