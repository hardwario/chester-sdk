/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_MICROSENS_180HS_H_
#define DRV_MICROSENS_180HS_H_

/**
 * @file drv_microsens_180hs.h
 * @brief MicroSENS 180-HS CO2 sensor driver
 *
 * This driver communicates with MicroSENS 180-HS via RS-232 (CHESTER-X12 module).
 * Protocol: Binary command/response with STX/ETX framing.
 */

#include "drv_interface.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MicroSENS 180-HS sensor data structure */
struct app_data_microsens {
	float co2_percent;      /* CO2 concentration in Vol.-% */
	float temperature_c;    /* Temperature in Celsius */
	float pressure_mbar;    /* Pressure in mbar */
	bool valid;             /* true if last sample was successful */
	uint32_t last_sample;   /* Timestamp of last sample (k_uptime_get_32) */
	uint32_t error_count;   /* Consecutive error count */
};

/* Sample entry for CBOR buffer */
struct microsens_sample {
	uint64_t timestamp;
	float co2_percent;
	float temperature_c;
	float pressure_mbar;
};

/**
 * @brief Get pointer to MicroSENS data
 *
 * Returns pointer to the microsens data structure in g_app_data.
 *
 * @return Pointer to app_data_microsens structure
 */
struct app_data_microsens *microsens_get_data(void);

int microsens_get_samples(struct microsens_sample *out, int max_count);
void microsens_clear_samples(void);
int microsens_get_sample_count(void);

/**
 * @brief MicroSENS driver instance
 *
 * Used by app_device.c for driver registration.
 */
extern const struct app_device_driver microsens_driver;

/**
 * @brief MicroSENS shell subcommands
 *
 * Used by app_device.c for shell command registration.
 * Provides: sample, calibrate, reset
 */
extern const union shell_cmd_entry microsens_shell_cmds;

#ifdef __cplusplus
}
#endif

#endif /* DRV_MICROSENS_180HS_H_ */
