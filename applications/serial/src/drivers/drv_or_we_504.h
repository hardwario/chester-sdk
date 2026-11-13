/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_OR_WE_504_H_
#define DRV_OR_WE_504_H_

/**
 * @file drv_or_we_504.h
 * @brief Orno OR-WE-504 single-phase energy meter driver
 *
 * Register map (per OR-WE-504 datasheet):
 * - Voltage:          0x0000 (UINT16, /10 V)
 * - Current:          0x0001 (UINT16, /10 A)
 * - Frequency:        0x0002 (UINT16, /10 Hz)
 * - Active power:     0x0003 (INT16, W)
 * - Reactive power:   0x0004 (INT16, var)
 * - Apparent power:   0x0005 (UINT16, VA)
 * - Power factor:     0x0006 (INT16, /1000)
 * - Active energy:    0x0007 (UINT32 BE Swapped long, Wh)
 * - Reactive energy:  0x0009 (UINT32 BE Swapped long, varh)
 *
 * NOTE: meter is unidirectional — no energy export register.
 * Communication: 9600 baud, 8N1 (accepts 8E1)
 * The address (0x000F) is written with FC16 after an unlock with function 0x28, see
 * app_modbus_change_addr_or_we_504().
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OR-WE-504 data structure (1-phase, 9 values) — units kW/kWh for consistency across drivers */
struct app_data_or_we_504 {
	float voltage;         /* V */
	float current;         /* A */
	float frequency;       /* Hz */
	float power;           /* kW (signed) */
	float power_reactive;  /* kvar (signed) */
	float power_apparent;  /* kVA */
	float power_factor;    /* dimensionless (-1..1) */
	float energy_in;       /* kWh, total active (import only — meter is unidirectional) */
	float energy_reactive; /* kvarh, total reactive */
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample record (slot record ring, encoded to CBOR) */
struct or_we_504_sample {
	uint64_t timestamp;
	float voltage;
	float current;
	float frequency;
	float power;
	float power_reactive;
	float power_apparent;
	float power_factor;
	float energy_in;
};

struct app_driver_slot;

/**
 * @brief Get the latest OR-WE-504 data of a slot (caller holds slot->lock)
 */
const struct app_data_or_we_504 *or_we_504_get_data(const struct app_driver_slot *slot);

#ifdef __cplusplus
}
#endif

#endif /* DRV_OR_WE_504_H_ */
