/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_PROMAG_MF7S_H_
#define DRV_PROMAG_MF7S_H_

/**
 * @file drv_promag_mf7s.h
 * @brief Promag MF7S RFID reader driver
 *
 * This driver communicates with Promag MF7S via RS-232 (19200, 8N1).
 * The reader automatically outputs card UIDs when a card is presented.
 * Protocol: STX + 8 hex chars (UID) + CR + LF + ETX
 */

#include "drv_interface.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Promag MF7S data structure */
struct app_data_promag_mf7s {
	uint32_t last_uid;
	uint32_t last_read_ts; /* Unix timestamp of last card read */
	uint32_t total_reads;
	uint32_t error_count;
	bool sampling_active;
	bool valid;
};

/* Sample entry for CBOR buffer */
struct promag_mf7s_sample {
	uint64_t timestamp;
	uint32_t uid;
};

/**
 * @brief Get pointer to Promag MF7S data
 *
 * @return Pointer to app_data_promag_mf7s structure
 */
const struct app_data_promag_mf7s *promag_mf7s_get_data(void);

int promag_mf7s_get_samples(struct promag_mf7s_sample *out, int max_count);
void promag_mf7s_clear_samples(void);
int promag_mf7s_get_sample_count(void);

/**
 * @brief Promag MF7S driver instance
 */
extern const struct app_device_driver promag_mf7s_driver;

/**
 * @brief Promag MF7S shell subcommands
 *
 * Provides: firmware, notify, sampling
 */
extern const union shell_cmd_entry promag_mf7s_shell_cmds;

#ifdef __cplusplus
}
#endif

#endif /* DRV_PROMAG_MF7S_H_ */
