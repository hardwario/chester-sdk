/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_FLOWT_FT201_H_
#define DRV_FLOWT_FT201_H_

/**
 * @file drv_flowt_ft201.h
 * @brief FlowT FT201 ultrasonic flowmeter driver
 *
 * Communication: Modbus RTU, 9600 8N1 (default).
 * Data format: Float32 LSW (low word first / word swap).
 * Totalizers: mantissa (Float32 LSW) * 10^exponent (UINT16).
 */

#include "drv_interface.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FlowT FT201 data structure */
struct app_data_flowt_ft201 {
	float flow_s;          /* Flow m3/s */
	float flow_m;          /* Flow m3/min */
	float flow_h;          /* Flow m3/h */
	float velocity;        /* Velocity m/s */
	float total_positive;  /* Positive totalizer (mantissa * 10^exp) */
	float total_negative;  /* Negative totalizer */
	float total_net;       /* Net totalizer */
	float energy_hot;      /* Energy total hot (mantissa * 10^exp) */
	float energy_cold;     /* Energy total cold */
	float temp_inlet;      /* Temperature inlet (C) */
	float temp_outlet;     /* Temperature outlet (C) */
	uint8_t signal_quality; /* Signal quality 0-99 */
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer */
struct flowt_ft201_sample {
	uint64_t timestamp;
	float flow_s;
	float flow_m;
	float flow_h;
	float velocity;
	float total_positive;
	float total_negative;
	float total_net;
	float energy_hot;
	float energy_cold;
	float temp_inlet;
	float temp_outlet;
	uint8_t signal_quality;
};

const struct app_data_flowt_ft201 *flowt_ft201_get_data(void);
void flowt_ft201_set_addr(uint8_t addr);
uint8_t flowt_ft201_get_addr(void);

int flowt_ft201_get_samples(struct flowt_ft201_sample *out, int max_count);
void flowt_ft201_clear_samples(void);
int flowt_ft201_get_sample_count(void);

extern const struct app_device_driver flowt_ft201_driver;
extern const union shell_cmd_entry flowt_ft201_shell_cmds;

#ifdef __cplusplus
}
#endif

#endif /* DRV_FLOWT_FT201_H_ */
