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
 * Requires unlock (function 0x28) before configuration changes.
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
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer */
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

/**
 * @brief Get OR-WE-504 data
 *
 * @return Pointer to data structure, or NULL if not available
 */
const struct app_data_or_we_504 *or_we_504_get_data(void);

/**
 * @brief Set Modbus address for OR-WE-504
 *
 * @param addr Modbus address (1-247)
 */
void or_we_504_set_addr(uint8_t addr);

/**
 * @brief Get current Modbus address
 *
 * @return Modbus address
 */
uint8_t or_we_504_get_addr(void);

/**
 * @brief Get buffered samples for CBOR encoding
 *
 * @param out Output buffer for samples
 * @param max_count Maximum number of samples to retrieve
 * @return Number of samples copied
 */
int or_we_504_get_samples(struct or_we_504_sample *out, int max_count);

/**
 * @brief Clear sample buffer (call after successful upload)
 */
void or_we_504_clear_samples(void);

/**
 * @brief Get current sample count in buffer
 *
 * @return Number of samples in buffer
 */
int or_we_504_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_OR_WE_504_H_ */
