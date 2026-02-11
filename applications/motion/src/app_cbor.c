/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>

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
/* ^^^ Preserved code "functions" (end) */

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

static int encode(zcbor_state_t *zs)
{
	int ret;
	uint64_t timestamp;


	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	/* ### Preserved code "message" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__VERSION);
		zcbor_uint32_put(zs, 2);

		static uint32_t sequence;
		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__SEQUENCE);
		zcbor_uint32_put(zs, sequence++);

		/*uint64_t timestamp;
		ret = ctr_rtc_get_ts(&timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}
*/
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

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__VOLTAGE_LOAD);
		if (isnan(g_app_data.system_voltage_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_load);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__VOLTAGE_REST);
		if (isnan(g_app_data.system_voltage_rest)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_voltage_rest);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_SYSTEM__CURRENT_LOAD);
		if (isnan(g_app_data.system_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, g_app_data.system_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "system" (end) */

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
				LOG_ERR("Call `ctr_lte_v2_get_conn_param` failed: %d", ret);
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



#if defined(CONFIG_CTR_S3)

    /* ### Preserved code "motion" (begin) */
    zcbor_uint32_put(zs, CODEC_KEY_E_MOTION);
    {
       zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);


       zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__TOTALIZER);
       {
           zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

           zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__TOTALIZER__DETECT_LEFT);
           zcbor_uint32_put(zs, g_app_data.total_detect_left);

           zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__TOTALIZER__DETECT_RIGHT);
           zcbor_uint32_put(zs, g_app_data.total_detect_right);

           zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__TOTALIZER__MOTION_LEFT);
           zcbor_uint32_put(zs, g_app_data.total_motion_left);

           zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__TOTALIZER__MOTION_RIGHT);
           zcbor_uint32_put(zs, g_app_data.total_motion_right);

           zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
       }


       zcbor_uint32_put(zs, CODEC_KEY_E_MOTION__SAMPLES);
       {

           zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

           if (g_app_data.motion_sample_count > 0) {

               int64_t timestamp_abs = g_app_data.motion_samples[0].timestamp;

               zcbor_int64_put(zs, timestamp_abs);


               for (int i = 0; i < g_app_data.motion_sample_count; i++) {

                   int64_t timestamp_offset = g_app_data.motion_samples[i].timestamp - timestamp_abs;


                   zcbor_int64_put(zs, timestamp_offset);


                   zcbor_uint32_put(zs, g_app_data.motion_samples[i].value.detect_left);
                   zcbor_uint32_put(zs, g_app_data.motion_samples[i].value.detect_right);
                   zcbor_uint32_put(zs, g_app_data.motion_samples[i].value.motion_left);
                   zcbor_uint32_put(zs, g_app_data.motion_samples[i].value.motion_right);
               }
           }


           zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
       }

       zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
    }
#endif /* defined(CONFIG_CTR_S3) */


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
