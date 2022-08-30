#include "app_cbor.h"
#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <ctr_info.h>
#include <ctr_rtc.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

#if defined(CONFIG_SHIELD_CTR_DS18B20)

static int compare(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;

	return (fa > fb) - (fa < fb);
}

static void aggregate(float *samples, size_t count, float *minimum, float *maximum, float *average,
                      float *median)
{
	if (count < 1) {
		*minimum = NAN;
		*maximum = NAN;
		*average = NAN;
		*median = NAN;

		return;
	}

	qsort(samples, count, sizeof(float), compare);

	*minimum = samples[0];
	*maximum = samples[count - 1];

	double average_ = 0;
	for (size_t i = 0; i < count; i++) {
		average_ += samples[i];
	}
	average_ /= count;

	*average = average_;
	*median = samples[count / 2];
}

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

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

#if defined(CONFIG_SHIELD_CTR_S1)
	zcbor_uint32_put(zs, MSG_KEY_S1);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		if (isnan(g_app_data.s1_temperature)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.s1_temperature * 100.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_HUMIDITY);
		if (isnan(g_app_data.s1_humidity)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.s1_humidity * 100.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_CO2_CONCENTRATION);
		if (isnan(g_app_data.s1_co2_concentration)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.s1_co2_concentration);
		}

		zcbor_uint32_put(zs, MSG_KEY_ALTITUDE);
		if (isnan(g_app_data.s1_altitude)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.s1_altitude);
		}

		zcbor_uint32_put(zs, MSG_KEY_PRESSURE);
		if (isnan(g_app_data.s1_pressure)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.s1_pressure);
		}

		zcbor_uint32_put(zs, MSG_KEY_ILLUMINANCE);
		if (isnan(g_app_data.s1_illuminance)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.s1_illuminance);
		}

		zcbor_uint32_put(zs, MSG_KEY_PIR_MOTION_COUNT);
		zcbor_uint32_put(zs, g_app_data.s1_pir_motion_count);

		zcbor_uint32_put(zs, MSG_KEY_BUTTON_COUNT);
		zcbor_uint32_put(zs, g_app_data.s1_button_count);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	zcbor_uint32_put(zs, MSG_KEY_HYGROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		if (isnan(g_app_data.hygro_temperature)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, g_app_data.hygro_temperature * 100.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_HUMIDITY);
		if (isnan(g_app_data.hygro_humidity)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.hygro_humidity * 100.f);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

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

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	zcbor_uint32_put(zs, MSG_KEY_W1_THERMOMETERS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		for (size_t i = 0; i < W1_THERM_COUNT; i++) {
			struct w1_therm *therm = &g_app_data.w1_therm[i];

			if (!therm->serial_number) {
				continue;
			}

			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint32_put(zs, MSG_KEY_SERIAL_NUMBER);
			zcbor_uint64_put(zs, therm->serial_number);

			zcbor_uint32_put(zs, MSG_KEY_TIMESTAMP);
			if (therm->sample_count) {
				zcbor_uint64_put(zs, therm->timestamp);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			float *samples = therm->samples;

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
			if (therm->sample_count) {
				float sample = samples[therm->sample_count - 1];
				zcbor_int32_put(zs, sample * 100.f);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			float min, max, avg, mdn;
			aggregate(therm->samples, therm->sample_count, &min, &max, &avg, &mdn);

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE_MIN);
			if (therm->sample_count) {
				zcbor_int32_put(zs, min * 100.f);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE_MAX);
			if (therm->sample_count) {
				zcbor_int32_put(zs, max * 100.f);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE_AVG);
			if (therm->sample_count) {
				zcbor_int32_put(zs, avg * 100.f);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE_MDN);
			if (therm->sample_count) {
				zcbor_int32_put(zs, mdn * 100.f);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_SAMPLE_COUNT);
			zcbor_uint32_put(zs, therm->sample_count);

			therm->sample_count = 0;

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}
