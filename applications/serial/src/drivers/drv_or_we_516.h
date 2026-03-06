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
	float voltage_l1, current_l1, power_l1, energy_l1;
	float voltage_l2, current_l2, power_l2, energy_l2;
	float voltage_l3, current_l3, power_l3, energy_l3;
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer */
struct or_we_516_sample {
	uint64_t timestamp;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power_l1, power_l2, power_l3;
	float power;
	float energy;
};

const struct app_data_or_we_516 *or_we_516_get_data(void);
void or_we_516_set_addr(uint8_t addr);
uint8_t or_we_516_get_addr(void);

int or_we_516_get_samples(struct or_we_516_sample *out, int max_count);
void or_we_516_clear_samples(void);
int or_we_516_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_OR_WE_516_H_ */
