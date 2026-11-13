/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_EM1XX_H_
#define DRV_EM1XX_H_

/**
 * @file drv_em1xx.h
 * @brief Carlo Gavazzi EM1XX (EM111) single-phase energy meter driver
 *
 * Register map (INT32 Low Word First):
 * - Current:        0x0100 (/1000 A)
 * - Voltage:        0x0102 (/10 V)
 * - Power:          0x0106 (/10000 kW)
 * - Apparent power: 0x0108 (/10000 kVA)
 * - Reactive power: 0x010A (/10000 kvar)
 * - Power factor:   0x010C (/1000 PF)
 * - Frequency:      0x0110 (/10 Hz)
 * - Energy IN:      0x0112 (/10 kWh)
 * - Energy OUT:     0x0116 (/10 kWh)
 *
 * Communication: 9600 baud, 8E1
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EM1XX data structure (1-phase, 9 values) */
struct app_data_em1xx {
	float current;        /* A */
	float voltage;        /* V */
	float power;          /* kW */
	float power_apparent; /* kVA */
	float power_reactive; /* kvar */
	float power_factor;   /* PF (-1..1) */
	float frequency;      /* Hz */
	float energy_in;      /* kWh */
	float energy_out;     /* kWh */
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample record (slot record ring, encoded to CBOR) */
struct em1xx_sample {
	uint64_t timestamp;
	float voltage;
	float current;
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
const struct app_data_em1xx *em1xx_get_data(const struct app_driver_slot *slot);

#ifdef __cplusplus
}
#endif

#endif /* DRV_EM1XX_H_ */
