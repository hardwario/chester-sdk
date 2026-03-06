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

/* EM5XX data structure (3-phase, 15 values) */
struct app_data_em5xx {
	float current;
	float voltage;
	float power;
	float frequency;
	float energy_in;
	float energy_out;
	float voltage_l1, current_l1, power_l1;
	float voltage_l2, current_l2, power_l2;
	float voltage_l3, current_l3, power_l3;
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer */
struct em5xx_sample {
	uint64_t timestamp;
	float voltage;
	float current;
	float power;
	float frequency;
	float energy_in;
	float energy_out;
	float voltage_l1, voltage_l2, voltage_l3;
	float current_l1, current_l2, current_l3;
	float power_l1, power_l2, power_l3;
};

const struct app_data_em5xx *em5xx_get_data(void);
void em5xx_set_addr(uint8_t addr);
uint8_t em5xx_get_addr(void);

int em5xx_get_samples(struct em5xx_sample *out, int max_count);
void em5xx_clear_samples(void);
int em5xx_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_EM5XX_H_ */
