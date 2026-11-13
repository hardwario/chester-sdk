/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_IEM3000_H_
#define DRV_IEM3000_H_

/**
 * @file drv_iem3000.h
 * @brief Schneider Electric iEM3000 3-phase energy meter driver
 *
 * Registers use Float32 (word swap) and INT64 for energy.
 * Communication: 9600 baud, 8E1
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* iEM3000 data structure (3-phase, 22 values) */
struct app_data_iem3000 {
	float current;
	float frequency;
	float energy_in;
	float energy_out;
	float energy_reactive_in;
	float energy_reactive_out;
	float power;
	float power_reactive; /* sum, kvar */
	float power_apparent; /* sum, kVA */
	float power_factor;   /* sum PF */
	float voltage_l1, current_l1, power_l1;
	float voltage_l2, current_l2, power_l2;
	float voltage_l3, current_l3, power_l3;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample record (slot record ring, encoded to CBOR) */
struct iem3000_sample {
	uint64_t timestamp;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power_l1, power_l2, power_l3;
	float power;
	float power_apparent;
	float power_reactive;
	float power_factor;
	float frequency;
	float energy_in;
	float energy_out;
};

struct app_driver_slot;

/* Latest data of a slot (caller holds slot->lock) */
const struct app_data_iem3000 *iem3000_get_data(const struct app_driver_slot *slot);

#ifdef __cplusplus
}
#endif

#endif /* DRV_IEM3000_H_ */
