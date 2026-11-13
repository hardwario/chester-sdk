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
#include "app_driver_store.h"
#include "drivers/drv_interface.h"
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
#include "drivers/drv_piketronic_rpp.h"
#include "drivers/drv_piketronic_rppu.h"
#include "drivers/drv_solax_g3.h"

/* CHESTER includes */
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_power.h>
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

static int encode_device_microsens(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				   uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct microsens_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CO2_PERCENT);
		if (!isnan(sample->co2_percent)) {
			zcbor_int32_put(zs, (int32_t)(sample->co2_percent * 1000.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMPERATURE);
		if (!isnan(sample->temperature_c)) {
			zcbor_int32_put(zs, (int32_t)(sample->temperature_c * 100.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__PRESSURE);
		if (!isnan(sample->pressure_mbar)) {
			zcbor_int32_put(zs, (int32_t)(sample->pressure_mbar * 10.f));
		} else {
			zcbor_nil_put(zs, NULL);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_or_we_504(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				   uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct or_we_504_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE);
		zcbor_int32_put(zs, (int32_t)(sample->voltage * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(sample->current * 1000.f));

		/* Power/energy/PF as native float32 (kW, kVA, kvar, kWh, dimensionless) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_float32_put(zs, sample->power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_APPARENT);
		zcbor_float32_put(zs, sample->power_apparent);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_REACTIVE);
		zcbor_float32_put(zs, sample->power_reactive);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR);
		zcbor_float32_put(zs, sample->power_factor);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(sample->frequency * 100.f));

		/* OR-WE-504 reads only active import (no export register) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_float32_put(zs, sample->energy_in);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_or_we_516(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				   uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct or_we_516_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(sample->current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(sample->current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(sample->current_l3 * 1000.f));

		/* OR-WE-516 power/energy as native float32 (kW, kVA, kvar, kWh) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L1);
		zcbor_float32_put(zs, sample->power_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L2);
		zcbor_float32_put(zs, sample->power_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L3);
		zcbor_float32_put(zs, sample->power_l3);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_float32_put(zs, sample->power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_APPARENT);
		zcbor_float32_put(zs, sample->power_apparent);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_REACTIVE);
		zcbor_float32_put(zs, sample->power_reactive);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR);
		zcbor_float32_put(zs, sample->power_factor);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L1);
		zcbor_float32_put(zs, sample->power_factor_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L2);
		zcbor_float32_put(zs, sample->power_factor_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L3);
		zcbor_float32_put(zs, sample->power_factor_l3);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(sample->frequency * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY);
		zcbor_float32_put(zs, sample->energy);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_float32_put(zs, sample->energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_float32_put(zs, sample->energy_out);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_em1xx(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
			       uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct em1xx_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE);
		zcbor_int32_put(zs, (int32_t)(sample->voltage * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(sample->current * 1000.f));

		/* EM1XX power/energy/PF as native float32 (kW, kVA, kvar, kWh, dim) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_float32_put(zs, sample->power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_APPARENT);
		zcbor_float32_put(zs, sample->power_apparent);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_REACTIVE);
		zcbor_float32_put(zs, sample->power_reactive);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR);
		zcbor_float32_put(zs, sample->power_factor);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(sample->frequency * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_float32_put(zs, sample->energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_float32_put(zs, sample->energy_out);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_em5xx(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
			       uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct em5xx_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT);
		zcbor_int32_put(zs, (int32_t)(sample->current * 1000.f));

		/* EM5XX power/energy/PF as native float32 (kW, kVA, kvar, kWh, dim) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_float32_put(zs, sample->power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_APPARENT);
		zcbor_float32_put(zs, sample->power_apparent);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_REACTIVE);
		zcbor_float32_put(zs, sample->power_reactive);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR);
		zcbor_float32_put(zs, sample->power_factor);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(sample->frequency * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_float32_put(zs, sample->energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_float32_put(zs, sample->energy_out);

		/* Per-phase data */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(sample->current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(sample->current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(sample->current_l3 * 1000.f));

		/* Per-phase power as native float32 (kW) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L1);
		zcbor_float32_put(zs, sample->power_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L2);
		zcbor_float32_put(zs, sample->power_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L3);
		zcbor_float32_put(zs, sample->power_l3);

		/* Per-phase power factor as native float32 */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L1);
		zcbor_float32_put(zs, sample->power_factor_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L2);
		zcbor_float32_put(zs, sample->power_factor_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR_L3);
		zcbor_float32_put(zs, sample->power_factor_l3);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_iem3000(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				 uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct iem3000_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L1);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l1 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L2);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l2 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VOLTAGE_L3);
		zcbor_int32_put(zs, (int32_t)(sample->voltage_l3 * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L1);
		zcbor_int32_put(zs, (int32_t)(sample->current_l1 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L2);
		zcbor_int32_put(zs, (int32_t)(sample->current_l2 * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CURRENT_L3);
		zcbor_int32_put(zs, (int32_t)(sample->current_l3 * 1000.f));

		/* iEM3000 power/energy/PF as native float32 (kW, kVA, kvar, kWh, dim) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L1);
		zcbor_float32_put(zs, sample->power_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L2);
		zcbor_float32_put(zs, sample->power_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_L3);
		zcbor_float32_put(zs, sample->power_l3);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER);
		zcbor_float32_put(zs, sample->power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_APPARENT);
		zcbor_float32_put(zs, sample->power_apparent);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_REACTIVE);
		zcbor_float32_put(zs, sample->power_reactive);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__POWER_FACTOR);
		zcbor_float32_put(zs, sample->power_factor);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FREQUENCY);
		zcbor_int32_put(zs, (int32_t)(sample->frequency * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_IN);
		zcbor_float32_put(zs, sample->energy_in);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_OUT);
		zcbor_float32_put(zs, sample->energy_out);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_promag_mf7s(zcbor_state_t *zs, struct app_driver_slot *slot,
				     uint32_t first, uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct promag_mf7s_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CARD_UID);
		zcbor_uint32_put(zs, sample->uid);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_flowt_ft201(zcbor_state_t *zs, struct app_driver_slot *slot,
				     uint32_t first, uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct flowt_ft201_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_S);
		zcbor_int32_put(zs, (int32_t)(sample->flow_s * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_M);
		zcbor_int32_put(zs, (int32_t)(sample->flow_m * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FLOW_H);
		zcbor_int32_put(zs, (int32_t)(sample->flow_h * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__VELOCITY);
		zcbor_int32_put(zs, (int32_t)(sample->velocity * 10000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_POSITIVE);
		zcbor_int64_put(zs, (int64_t)(sample->total_positive * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_NEGATIVE);
		zcbor_int64_put(zs, (int64_t)(sample->total_negative * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TOTAL_NET);
		zcbor_int64_put(zs, (int64_t)(sample->total_net * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_HOT);
		zcbor_int64_put(zs, (int64_t)(sample->energy_hot * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ENERGY_COLD);
		zcbor_int64_put(zs, (int64_t)(sample->energy_cold * 1000.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMP_INLET);
		zcbor_int32_put(zs, (int32_t)(sample->temp_inlet * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMP_OUTLET);
		zcbor_int32_put(zs, (int32_t)(sample->temp_outlet * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__SIGNAL_QUALITY);
		zcbor_uint32_put(zs, sample->signal_quality);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_piketronic_rpp(zcbor_state_t *zs, struct app_driver_slot *slot,
					uint32_t first, uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct piketronic_rpp_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__RADON_CONCENTRATION);
		zcbor_uint32_put(zs, sample->concentration);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__RADON_CONCENTRATION_DAY);
		zcbor_uint32_put(zs, sample->concentration_day);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMPERATURE);
		zcbor_int32_put(zs, (int32_t)sample->temperature * 100);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__HUMIDITY);
		zcbor_uint32_put(zs, sample->humidity);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

/* RPP-U reports through the same four keys as the RPP-R (see encode above) and adds the
 * accumulator voltage. The averaging window behind RADON_CONCENTRATION differs - the RPP-U
 * sends its sensitive 0.5 h average where the RPP-R sends a 1 h one - but a consumer reads
 * that from the device's `type_name`, so it is not encoded into the key.
 */
static int encode_device_piketronic_rppu(zcbor_state_t *zs, struct app_driver_slot *slot,
					 uint32_t first, uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct piketronic_rppu_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__RADON_CONCENTRATION);
		zcbor_uint32_put(zs, sample->concentration);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__RADON_CONCENTRATION_DAY);
		zcbor_uint32_put(zs, sample->concentration_day);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TEMPERATURE);
		zcbor_int32_put(zs, (int32_t)sample->temperature * 100);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__HUMIDITY);
		zcbor_uint32_put(zs, sample->humidity);

		/* Accumulator voltage, in millivolts straight onto a key declared $div 1000
		 * $fpp 3, so the cloud reports the probe's own resolution (3.576 V) and matches
		 * what the LoRaWAN payload carries. It deliberately does NOT reuse the shared
		 * `voltage` key: that is $div 100 because the meters report mains voltage
		 * through it. The RPP-R has no equivalent - it is bus-powered.
		 */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__ACCUMULATOR_VOLTAGE);
		if (sample->voltage_mv == 0) {
			/* 0 means the probe did not report a voltage, which is not a flat
			 * accumulator - drv_piketronic_rppu.c skips the alert on the same
			 * grounds, and the LRW decoder reports null. Sent as a plain 0 it
			 * decodes to 0.000 V, so one `accumulator_voltage < 3.3` rule - the
			 * kind sharing a key name across both transports invites - would fire
			 * here while staying quiet on LoRaWAN for the same probe state.
			 */
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, (int32_t)sample->voltage_mv);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

static int encode_device_solax_g3(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				  uint32_t end)
{
	if (first == end) {
		return 0;
	}

	zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA);
	zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (uint32_t seq = first; seq != end; seq++) {
		const struct solax_g3_sample *sample = app_driver_rec_get(slot, seq);

		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__TIMESTAMP);
		zcbor_uint64_put(zs, sample->timestamp);

		/* Powers in W as raw int32 (negatives -> CBOR negative int) */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__PV1_POWER);
		zcbor_int32_put(zs, (int32_t)sample->pv1_power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__PV2_POWER);
		zcbor_int32_put(zs, (int32_t)sample->pv2_power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__BAT_POWER);
		zcbor_int32_put(zs, (int32_t)sample->bat_power);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__BAT_TEMP);
		zcbor_int32_put(zs, (int32_t)sample->bat_temp);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__BAT_SOC);
		zcbor_int32_put(zs, (int32_t)sample->bat_soc);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FEEDIN_POWER);
		zcbor_int32_put(zs, (int32_t)sample->feedin_power);

		/* Energies as int32(kWh x 100), codec applies $div:100 $fpp:2 */
		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__FEEDIN_ENERGY);
		zcbor_int32_put(zs, (int32_t)(sample->feedin_energy * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__CONSUME_ENERGY);
		zcbor_int32_put(zs, (int32_t)(sample->consume_energy * 100.f));

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__EPS_POWER_L1);
		zcbor_int32_put(zs, (int32_t)sample->eps_power_l1);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__EPS_POWER_L2);
		zcbor_int32_put(zs, (int32_t)sample->eps_power_l2);

		zcbor_uint32_put(zs, CODEC_KEY_E_DEVICES__DATA__EPS_POWER_L3);
		zcbor_int32_put(zs, (int32_t)sample->eps_power_l3);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

/* value * mul rounded to int32; nil if not available or out of the int32 range */
static void put_value_mul(zcbor_state_t *zs, float value, float mul)
{
	int32_t v;

	if (app_driver_value_to_int32(value, mul, &v)) {
		zcbor_int32_put(zs, v);
	} else {
		zcbor_nil_put(zs, NULL);
	}
}

static void put_sample_mul(zcbor_state_t *zs, const struct ctr_data_aggreg *sample, float mul)
{
	put_value_mul(zs, sample->min, mul);
	put_value_mul(zs, sample->max, mul);
	put_value_mul(zs, sample->avg, mul);
	put_value_mul(zs, sample->mdn, mul);
}

/*
 * Items of a table-driven device, one $tsp list per item:
 *   <group>: { <item>: { measurements: [ts, interval, min, max, avg, mdn, ...] } }  (STATS)
 *   <group>: { <item>: { measurements: [ts, interval, value, ...] } }               (others)
 * Caller holds slot->lock.
 */
static void encode_device_items(zcbor_state_t *zs, struct app_driver_slot *slot, uint32_t first,
				uint32_t end)
{
	const struct app_device_driver *drv = slot->drv;

	if (first == end) {
		return;
	}

	zcbor_uint32_put(zs, drv->codec_key);
	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	for (size_t i = 0; i < drv->item_count; i++) {
		const struct app_driver_item *item = &drv->items[i];

		zcbor_uint32_put(zs, item->key);
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, item->key_measurements);
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint64_put(zs, app_driver_win_ts(slot, first));
		zcbor_uint32_put(zs, slot->items.interval);

		for (uint32_t seq = first; seq != end; seq++) {
			if (item->mode == APP_DRIVER_AGGREG_STATS) {
				put_sample_mul(zs, app_driver_win_stats(slot, i, seq), item->mul);
			} else {
				put_value_mul(zs, app_driver_win_value(slot, i, seq), item->mul);
			}
		}

		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
}

/* Type-specific measurements of a device; caller holds slot->lock */
static void encode_device_data(zcbor_state_t *zs, int idx, enum app_device_type type,
			       struct app_driver_slot *slot, const struct app_send_select *sel)
{
	/* Entries may have been dropped from a full ring since the selection was made */
	uint32_t first = sel->rec[idx].first;
	uint32_t end = sel->rec[idx].end;

	if (slot->drv->items) {
		first = sel->win[idx].first;
		end = sel->win[idx].end;

		app_driver_seq_clamp(&first, &end, app_driver_win_first(slot),
				     app_driver_win_end(slot));
		encode_device_items(zs, slot, first, end);
		return;
	}

	app_driver_seq_clamp(&first, &end, app_driver_rec_first(slot), app_driver_rec_end(slot));

	switch (type) {
	case APP_DEVICE_TYPE_MICROSENS_180HS:
		encode_device_microsens(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_OR_WE_504:
		encode_device_or_we_504(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_OR_WE_516:
		encode_device_or_we_516(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_EM1XX:
		encode_device_em1xx(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_EM5XX:
		encode_device_em5xx(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_IEM3000:
		encode_device_iem3000(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_PROMAG_MF7S:
		encode_device_promag_mf7s(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_FLOWT_FT201:
		encode_device_flowt_ft201(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_PIKETRONIC_RPP:
		encode_device_piketronic_rpp(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_PIKETRONIC_RPPU:
		encode_device_piketronic_rppu(zs, slot, first, end);
		break;
	case APP_DEVICE_TYPE_SOLAX_G3:
		encode_device_solax_g3(zs, slot, first, end);
		break;
	default:
		/* Unknown device type - skip measurements */
		break;
	}
}

static int encode_devices(zcbor_state_t *zs, const struct app_send_select *sel)
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
		struct app_driver_slot *slot = app_device_get_slot(i);

		if (slot) {
			app_driver_slot_lock(slot);
			encode_device_data(zs, i, type, slot, sel);
			app_driver_slot_unlock(slot);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	return 0;
}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

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

static int encode(zcbor_state_t *zs, const struct app_send_select *sel)
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
		zcbor_uint32_put(zs, sel->sequence);

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
	encode_devices(zs, sel);

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	/* BLE tags */
	if (sel->ble_tags) {
		encode_ble_tags(zs);
	}
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		int err = zcbor_pop_error(zs);

		/* Payload buffer exhausted: the caller splits the report */
		if (err == ZCBOR_ERR_NO_PAYLOAD) {
			return -ENOSPC;
		}

		LOG_ERR("Encoding failed: %d", err);
		return -EFAULT;
	}

	return 0;
}

int app_cbor_encode(zcbor_state_t *zs, const struct app_send_select *sel)
{
	int ret;

	app_data_lock();

	ret = encode(zs, sel);
	if (ret) {
		if (ret != -ENOSPC) {
			LOG_ERR("Call `encode` failed: %d", ret);
		}
		app_data_unlock();
		return ret;
	}

	app_data_unlock();

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */
