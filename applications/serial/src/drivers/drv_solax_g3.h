/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_SOLAX_G3_H_
#define DRV_SOLAX_G3_H_

/**
 * @file drv_solax_g3.h
 * @brief SolaX X3-Hybrid G3 hybrid inverter driver
 *
 * Modbus RTU, FC04 (Read Input Registers).
 * Communication: 115200 baud, 8N1 (slave id 1 default).
 * 32-bit values are LSW-first (low word at lower address).
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SolaX X3-Hybrid G3 data structure (customer + service sets) */
struct app_data_solax_g3 {
	/* Customer set (periodic -> CBOR / LoRaWAN) */
	float pv1_power, pv2_power;       /* W */
	float bat_power;                  /* W, +charge / -discharge */
	float bat_temp;                   /* degC */
	float bat_soc;                    /* % */
	float feedin_power;               /* W, +export / -import */
	float feedin_energy_total;        /* kWh */
	float consume_energy_total;       /* kWh */
	float eps_power_l1, eps_power_l2, eps_power_l3; /* W */

	/* Service set (on demand -> shell only) */
	float pv1_voltage, pv2_voltage;   /* V */
	float pv1_current, pv2_current;   /* A */
	float temperature;                /* degC (heatsink) */
	uint16_t run_mode;
	float bat_voltage;                /* V */
	float bat_current;                /* A */
	uint16_t bms_state;               /* 0=disconnected / 1=connected */
	float energy_out_total;           /* kWh */
	float energy_out_today;           /* kWh */
	uint16_t bms_warning_lsb, bms_warning_msb;
	float bms_charge_max, bms_discharge_max; /* A */
	uint16_t inv_fault_lsb, inv_fault_msb;
	uint16_t mgr_fault;
	float voltage_l1, voltage_l2, voltage_l3; /* V (R/S/T) */
	float current_l1, current_l2, current_l3; /* A */
	float power_l1, power_l2, power_l3;       /* W */
	float frequency;                  /* Hz */

	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;         /* customer set valid */
	bool service_valid; /* service set valid */
};

/* Sample entry for CBOR/LoRaWAN buffer (customer set only) */
struct solax_g3_sample {
	uint64_t timestamp;
	float pv1_power, pv2_power;
	float bat_power;
	float bat_temp;
	float bat_soc;
	float feedin_power;
	float feedin_energy;  /* kWh total */
	float consume_energy; /* kWh total */
	float eps_power_l1, eps_power_l2, eps_power_l3;
};

const struct app_data_solax_g3 *solax_g3_get_data(void);
void solax_g3_set_addr(uint8_t addr);
uint8_t solax_g3_get_addr(void);

/* Read the service set on demand (shell), stores into internal struct only */
int solax_g3_sample_service(void);

int solax_g3_get_samples(struct solax_g3_sample *out, int max_count);
void solax_g3_clear_samples(void);
int solax_g3_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_SOLAX_G3_H_ */
