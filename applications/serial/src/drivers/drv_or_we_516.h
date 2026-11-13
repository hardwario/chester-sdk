/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_OR_WE_516_H_
#define DRV_OR_WE_516_H_

/**
 * @file drv_or_we_516.h
 * @brief Orno OR-WE-516 3-phase energy meter driver
 *
 * All registers are Float32 Big Endian.
 * Communication: 9600 baud, 8E1 (hardware fixed)
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OR-WE-516 data structure (3-phase, 25 values) */
struct app_data_or_we_516 {
	float frequency;
	float energy;
	float energy_in;
	float energy_out;
	float energy_reactive;
	float energy_reactive_in;
	float energy_reactive_out;
	float power;
	float power_reactive;
	float power_apparent;
	float power_factor;
	float voltage_l1, current_l1, power_l1, energy_l1, power_factor_l1;
	float voltage_l2, current_l2, power_l2, energy_l2, power_factor_l2;
	float voltage_l3, current_l3, power_l3, energy_l3, power_factor_l3;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample record (slot record ring, encoded to CBOR) */
struct or_we_516_sample {
	uint64_t timestamp;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power_l1, power_l2, power_l3;
	float power;
	float power_apparent; /* sum, VA */
	float power_reactive; /* sum, VAr */
	float power_factor;   /* sum PF */
	float power_factor_l1, power_factor_l2, power_factor_l3;
	float frequency;
	float energy;         /* total kWh */
	float energy_in;      /* import kWh */
	float energy_out;     /* export kWh */
};

struct app_driver_slot;

/* Latest data of a slot (caller holds slot->lock) */
const struct app_data_or_we_516 *or_we_516_get_data(const struct app_driver_slot *slot);

#ifdef __cplusplus
}
#endif

#endif /* DRV_OR_WE_516_H_ */
