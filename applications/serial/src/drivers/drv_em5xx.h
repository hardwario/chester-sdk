/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_EM5XX_H_
#define DRV_EM5XX_H_

/**
 * @file drv_em5xx.h
 * @brief Carlo Gavazzi EM5XX (EM540) 3-phase energy meter driver
 *
 * All registers are INT32 Low Word First.
 * Communication: 9600 baud, 8E1
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EM5XX data structure (3-phase) */
struct app_data_em5xx {
	float current;
	float power;
	float power_apparent; /* sum, kVA */
	float power_reactive; /* sum, kvar */
	float power_factor;   /* sum PF */
	float frequency;
	float energy_in;
	float energy_out;
	float voltage_l1, current_l1, power_l1, power_factor_l1;
	float voltage_l2, current_l2, power_l2, power_factor_l2;
	float voltage_l3, current_l3, power_l3, power_factor_l3;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample record (slot record ring, encoded to CBOR) */
struct em5xx_sample {
	uint64_t timestamp;
	float current;
	float power;
	float power_apparent;
	float power_reactive;
	float power_factor;
	float frequency;
	float energy_in;
	float energy_out;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power_l1, power_l2, power_l3;
	float power_factor_l1, power_factor_l2, power_factor_l3;
};

struct app_driver_slot;

/* Latest data of a slot (caller holds slot->lock) */
const struct app_data_em5xx *em5xx_get_data(const struct app_driver_slot *slot);

#ifdef __cplusplus
}
#endif

#endif /* DRV_EM5XX_H_ */
