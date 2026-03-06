/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#if FEATURE_SUBSYSTEM_LTE_V2

#include "app_cbor.h"
#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "app_device.h"
#include "feature.h"

/* Driver includes for data access */
#include "drivers/drv_microsens_180hs.h"
#include "drivers/drv_or_we_504.h"
#include "drivers/drv_em1xx.h"
#include "drivers/drv_or_we_516.h"
#include "drivers/drv_em5xx.h"
#include "drivers/drv_iem3000.h"
#include "drivers/drv_promag_mf7s.h"
#include "drivers/drv_flowt_ft201.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_util.h>

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_buf.h>
#endif

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

/* Codec keys are now generated in app_codec.h by `west gen-codec` */

LOG_MODULE_REGISTER(app_cbor, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

static int encode_device_microsens(zcbor_state_t *zs, int device_idx)
{
	struct microsens_sample samples[MAX_SAMPLES];
	int count = microsens_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CO2_PERCENT);
		if (!isnan(samples[i].co2_percent)) {
			zcbor_int32_put(zs, (int32_t)(samples[i].co2_percent * 1000.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMPERATURE);
		if (!isnan(samples[i].temperature_c)) {
			zcbor_int32_put(zs, (int32_t)(samples[i].temperature_c * 100.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__PRESSURE);
		if (!isnan(samples[i].pressure_mbar)) {
			zcbor_int32_put(zs, (int32_t)(samples[i].pressure_mbar * 10.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_or_we_504(zcbor_state_t *zs, int device_idx)
{
	struct or_we_504_sample samples[MAX_SAMPLES];
	int count = or_we_504_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(samples[i].current * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_int32_put(zs, (int32_t)(samples[i].power * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_or_we_516(zcbor_state_t *zs, int device_idx)
{
	struct or_we_516_sample samples[MAX_SAMPLES];
	int count = or_we_516_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l3 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l1 * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l2 * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l3 * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_TOTAL);
		zcbor_int32_put(zs, (int32_t)(samples[i].power * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_em1xx(zcbor_state_t *zs, int device_idx)
{
	struct em1xx_sample samples[MAX_SAMPLES];
	int count = em1xx_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(samples[i].current * 1000.f));

		/* EM1XX power is in kW, convert to W * 10 */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_int32_put(zs, (int32_t)(samples[i].power * 1000.f * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(samples[i].frequency * 100.f));

		/* EM1XX energy is in kWh, convert to Wh */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_uint64_put(zs, (uint64_t)(samples[i].energy_in * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_uint64_put(zs, (uint64_t)(samples[i].energy_out * 1000.f));

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_em5xx(zcbor_state_t *zs, int device_idx)
{
	struct em5xx_sample samples[MAX_SAMPLES];
	int count = em5xx_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(samples[i].current * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_int32_put(zs, (int32_t)(samples[i].power * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(samples[i].frequency * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy_out);

		/* Per-phase data */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l3 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l1 * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l2 * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].power_l3 * 10.f));

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_iem3000(zcbor_state_t *zs, int device_idx)
{
	struct iem3000_sample samples[MAX_SAMPLES];
	int count = iem3000_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(samples[i].current_l3 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_TOTAL);
		zcbor_int32_put(zs, (int32_t)(samples[i].power * 10.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_uint64_put(zs, (uint64_t)samples[i].energy_out);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_promag_mf7s(zcbor_state_t *zs, int device_idx)
{
	struct promag_mf7s_sample samples[MAX_SAMPLES];
	int count = promag_mf7s_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CARD_UID);
		zcbor_uint32_put(zs, samples[i].uid);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_flowt_ft201(zcbor_state_t *zs, int device_idx)
{
	struct flowt_ft201_sample samples[MAX_SAMPLES];
	int count = flowt_ft201_get_samples(samples, MAX_SAMPLES);

	if (count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < count; i++) {
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, samples[i].timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_S);
		zcbor_int32_put(zs, (int32_t)(samples[i].flow_s * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_M);
		zcbor_int32_put(zs, (int32_t)(samples[i].flow_m * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_H);
		zcbor_int32_put(zs, (int32_t)(samples[i].flow_h * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VELOCITY);
		zcbor_int32_put(zs, (int32_t)(samples[i].velocity * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_POSITIVE);
		zcbor_int64_put(zs, (int64_t)(samples[i].total_positive * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_NEGATIVE);
		zcbor_int64_put(zs, (int64_t)(samples[i].total_negative * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_NET);
		zcbor_int64_put(zs, (int64_t)(samples[i].total_net * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_HOT);
		zcbor_int64_put(zs, (int64_t)(samples[i].energy_hot * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_COLD);
		zcbor_int64_put(zs, (int64_t)(samples[i].energy_cold * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMP_INLET);
		zcbor_int32_put(zs, (int32_t)(samples[i].temp_inlet * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMP_OUTLET);
		zcbor_int32_put(zs, (int32_t)(samples[i].temp_outlet * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__SIGNAL_QUALITY);
		zcbor_uint32_put(zs, samples[i].signal_quality);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_devices(zcbor_state_t *zs)
{
	int device_count = 0;

	/* Count configured devices */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_DEVICE_TYPE_NONE) {
			device_count++;
		}
	}

	if (device_count == 0) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = g_app_config.devices[i].type;

		if (type == APP_DEVICE_TYPE_NONE) {
			continue;
		}

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		/* Common device fields */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DEVICE);
		zcbor_uint32_put(zs, i);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__TYPE);
		zcbor_uint32_put(zs, type);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__TYPE_NAME);
		zcbor_tstr_put_term(zs, app_device_type_name(type), CONFIG_ZCBOR_MAX_STR_LEN);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__ADDR);
		zcbor_uint32_put(zs, g_app_config.devices[i].addr);

		/* Type-specific measurements */
		switch (type) {
		case APP_DEVICE_TYPE_MICROSENS_180HS:
			encode_device_microsens(zs, i);
			break;
		case APP_DEVICE_TYPE_OR_WE_504:
			encode_device_or_we_504(zs, i);
			break;
		case APP_DEVICE_TYPE_OR_WE_516:
			encode_device_or_we_516(zs, i);
			break;
		case APP_DEVICE_TYPE_EM1XX:
			encode_device_em1xx(zs, i);
			break;
		case APP_DEVICE_TYPE_EM5XX:
			encode_device_em5xx(zs, i);
			break;
		case APP_DEVICE_TYPE_IEM3000:
			encode_device_iem3000(zs, i);
			break;
		case APP_DEVICE_TYPE_PROMAG_MF7S:
			encode_device_promag_mf7s(zs, i);
			break;
		case APP_DEVICE_TYPE_FLOWT_FT201:
			encode_device_flowt_ft201(zs, i);
			break;
		default:
			/* Unknown device type - skip measurements */
			break;
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

static void put_sample_mul(zcbor_state_t *zs, struct ctr_data_aggreg *sample, float mul)
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

static int encode_ble_tags(zcbor_state_t *zs)
{
	zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS);
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
					zcbor_tstr_put_term(zs, addr_str, sizeof(addr_str));
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

				zcbor_uint32_put(zs, CODEC_KEY_E_BLE_TAGS__HUMIDITY);
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

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

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
		zcbor_uint32_put(zs, 2);

		zcbor_uint32_put(zs, CODEC_KEY_E_MESSAGE__SEQUENCE);
		static uint32_t sequence;
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

	/* ### Preserved code "attribute" (begin) */

	zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__VENDOR_NAME);
		zcbor_tstr_put_term(zs, vendor_name, CONFIG_ZCBOR_MAX_STR_LEN);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__PRODUCT_NAME);
		zcbor_tstr_put_term(zs, product_name, CONFIG_ZCBOR_MAX_STR_LEN);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__HW_VARIANT);
		zcbor_tstr_put_term(zs, hw_variant, CONFIG_ZCBOR_MAX_STR_LEN);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__HW_REVISION);
		zcbor_tstr_put_term(zs, hw_revision, CONFIG_ZCBOR_MAX_STR_LEN);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__FW_VERSION);
		zcbor_tstr_put_term(zs, fw_version, CONFIG_ZCBOR_MAX_STR_LEN);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		zcbor_uint32_put(zs, CODEC_KEY_E_ATTRIBUTE__SERIAL_NUMBER);
		zcbor_tstr_put_term(zs, serial_number, CONFIG_ZCBOR_MAX_STR_LEN);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	/* ^^^ Preserved code "attribute" (end) */

	/* ### Preserved code "system" (begin) */

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

	/* ^^^ Preserved code "accelerometer" (end) */

	/* Devices array */
	encode_devices(zs);

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	/* BLE tags */
	encode_ble_tags(zs);
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

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
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */
