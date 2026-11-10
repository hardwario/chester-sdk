/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_DRV_H_
#define APP_SENSOR_DRV_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pluggable "atypical sensor" layer for CHESTER Control.
 *
 * Each sensor type is a small self-contained driver (src/drivers/drv_<type>.c),
 * modeled on the serial app's device drivers. A sensor claims a power channel
 * and a measurement channel; the generic runtime (app_work.c) does the
 * power-on -> settle -> ADC read -> power-off lifecycle and hands the raw mV to
 * the driver's sample() for type-specific evaluation.
 *
 * Type ids are stable — they appear in the CBOR/LRW payload.
 */

enum app_sensor_type {
	APP_SENSOR_TYPE_NONE = 0,
	APP_SENSOR_TYPE_LD81 = 1,
	APP_SENSOR_TYPE_COUNT,
};

/* Each driver owns its decoded data internally (private static storage in
 * drv_<type>.c) and exposes it through a type-specific getter, e.g.
 * ld81_get_data(). The generic runtime never sees the type-specific fields —
 * it only drives the power/settle/read lifecycle and hands the raw mV (or an
 * invalidate) to the driver, indexed by slot. */
struct app_sensor_driver {
	const char *name;           /* config token, e.g. "ld-81" */
	uint8_t type_id;            /* enum app_sensor_type */
	bool needs_power;           /* claims a power channel */
	uint32_t power_on_delay_ms; /* extra settle beyond the generic default */

	/* Largest millivolt spread across a burst of reads that still counts as a
	 * real measurement, or 0 to skip the check. A node held by an excitation
	 * resistor and a sensor barely moves; a pin with nothing on it wanders by
	 * hundreds of millivolts. Without this, a sensor configured on the wrong
	 * channel — or one whose wire has come off — reads noise that can land above
	 * the flood threshold and is reported as a confident flood. */
	uint16_t max_spread_mv;

	int (*init)(int slot, uint8_t ch_power, uint8_t ch_measure);
	/* Store the decoded result for slot. Returns 0 on success, a negative
	 * errno on failure, or APP_SENSOR_SAMPLE_ALARM when the decoded state
	 * changed in a way that warrants reporting now rather than at the next
	 * periodic report. */
	int (*sample)(int slot, int raw_mv);
	/* Mark slot's data invalid after the runtime had to discard a sample (floating
	 * input, divider not engaged, X0 not responding, ADC error). Returns
	 * APP_SENSOR_SAMPLE_ALARM when losing the reading warrants a report now, as
	 * sample() does for a fault it finds itself, else 0. NULL ok. */
	int (*invalidate)(int slot);
	void (*print_data)(const struct shell *shell, int slot);
};

/* sample() return value requesting an immediate (rate-limited) report. */
#define APP_SENSOR_SAMPLE_ALARM 1

/* Identify which channel a sensor is physically wired to, by exciting one channel
 * and reading all the others at once. Pass a 1-based supply channel, or -1 to use the
 * configured sensor's. `slot_arg` is a 1-based sensor slot to judge the result against, or
 * -1 to resolve it from the supply channel. See the comment on the definition for why this
 * is a shipped command and not a bench script. */
int app_sensor_drv_scan(const struct shell *shell, int ch_power, int slot_arg);

/* Catalog lookup (dispatch-only, defined in app_sensor_drv.c). */
const struct app_sensor_driver *app_sensor_find_driver_by_name(const char *name);
const struct app_sensor_driver *app_sensor_find_driver_by_type(uint8_t type_id);

/* Runtime entry points. */
int app_sensor_drv_init(void);
int app_sensor_drv_sample_all(void);
/* Live-sample one configured slot. Returns APP_SENSOR_SAMPLE_ALARM when the slot's state
 * changed and should be reported now (a failed sample can do that too), 0 when it did not,
 * or a negative errno for a failed sample that changed nothing worth reporting. */
int app_sensor_drv_sample_slot(int slot);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_DRV_H_ */
