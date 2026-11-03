/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CTR_POWER_INTERNAL_H_
#define CTR_POWER_INTERNAL_H_

/* CHESTER includes */
#include <chester/ctr_power.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_power_data {
	enum ctr_power_source source;

	enum ctr_power_line line;
	/** Volts, NAN if unknown. */
	float line_voltage;
	/** Volts, NAN if unknown or unsupported by the source. */
	float backup_voltage;

	/** Volts, NAN if unknown. */
	float system_voltage_rest;
	/** Volts, NAN if unknown. */
	float system_voltage_load;
	/** Milliamperes, NAN if unknown. */
	float system_current_load;

	int event_count;
	struct ctr_power_event events[CONFIG_CTR_POWER_MAX_EVENTS];
};

/**
 * @brief Lock the internal data mutex and return the current data snapshot.
 *
 * Must be paired with a call to `ctr_power_data_unlock`.
 */
struct ctr_power_data *ctr_power_data_lock(void);

/**
 * @brief Unlock the internal data mutex previously locked by `ctr_power_data_lock`.
 */
void ctr_power_data_unlock(void);

/**
 * @brief Drop the buffered events once an encoder has consumed them.
 *
 * The caller must hold the lock taken by `ctr_power_data_lock`, so that no event recorded
 * after the encode can be dropped.
 */
void ctr_power_data_clear_events(struct ctr_power_data *data);

#ifdef __cplusplus
}
#endif

#endif /* CTR_POWER_INTERNAL_H_ */
