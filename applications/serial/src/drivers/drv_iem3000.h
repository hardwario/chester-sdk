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
	float power_reactive;
	float power_apparent;
	float power_factor;
	float voltage_l1, current_l1, power_l1;
	float voltage_l2, current_l2, power_l2;
	float voltage_l3, current_l3, power_l3;
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer */
struct iem3000_sample {
	uint64_t timestamp;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power;
	float energy_in;
	float energy_out;
};

const struct app_data_iem3000 *iem3000_get_data(void);
void iem3000_set_addr(uint8_t addr);
uint8_t iem3000_get_addr(void);

int iem3000_get_samples(struct iem3000_sample *out, int max_count);
void iem3000_clear_samples(void);
int iem3000_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_IEM3000_H_ */
