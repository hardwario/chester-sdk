/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_POWER_H_
#define CHESTER_INCLUDE_CTR_POWER_H_

#if defined(CONFIG_CTR_POWER_LRW)
/* CHESTER includes */
#include <chester/ctr_buf.h>
#endif /* defined(CONFIG_CTR_POWER_LRW) */

/* Zephyr includes */
#include <zephyr/kernel.h>

#if defined(CONFIG_CTR_POWER_CBOR)
#include <zcbor_common.h>
#endif /* defined(CONFIG_CTR_POWER_CBOR) */

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_power ctr_power
 * @{
 */

enum ctr_power_source {
	CTR_POWER_SOURCE_NONE = 0,
	CTR_POWER_SOURCE_CTR_Z = 1,
	CTR_POWER_SOURCE_CTR_X4_A = 2,
	CTR_POWER_SOURCE_CTR_X4_B = 3,
	CTR_POWER_SOURCE_CTR_X10 = 4,
};

/* Values deliberately match `enum ctr_x4_line_state`. */
enum ctr_power_line {
	CTR_POWER_LINE_UNKNOWN = 0,
	CTR_POWER_LINE_DISCONNECTED = 1,
	CTR_POWER_LINE_CONNECTED = 2,
};

struct ctr_power_event {
	/** Unix timestamp, 0 when the RTC has no valid time. */
	int64_t timestamp;
	enum ctr_power_line line;
};

typedef void (*ctr_power_event_cb)(const struct ctr_power_event *event, void *user_data);

/**
 * @brief Detect the power source and start sampling.
 *
 * Line state changes are buffered as events until the next successful encode
 * (`ctr_power_cbor_encode()` or `ctr_power_lrw_encode*()`), which consumes them.
 *
 * @param cb Called when the line state changes and reporting is enabled for
 *           that transition. Runs in the system workqueue context. May be NULL.
 */
int ctr_power_init(ctr_power_event_cb cb, void *user_data);

/**
 * @brief Get the detected external power source.
 *
 * The source is fixed by `ctr_power_init()`; before that it reads as
 * `CTR_POWER_SOURCE_NONE`.
 */
enum ctr_power_source ctr_power_get_source(void);

#if defined(CONFIG_CTR_POWER_CBOR)

/**
 * @brief Encode the current power data into a CBOR map under the `power` key.
 *
 * On success, the encoded events are removed from the event buffer; events
 * recorded after the encode are kept for the next one.
 *
 * @param zs zcbor encoding state, positioned inside the top-level map.
 */
int ctr_power_cbor_encode(zcbor_state_t *zs);

#endif /* defined(CONFIG_CTR_POWER_CBOR) */

#if defined(CONFIG_CTR_POWER_LRW)

/**
 * @brief Encode the current system power data into a LoRaWAN payload.
 *
 * Appends 5 bytes: `system_voltage_rest` (u16, little-endian, millivolts),
 * `system_voltage_load` (u16, little-endian, millivolts), `system_current_load`
 * (u8, milliamperes). Unknown values are encoded as `BIT_MASK(16)` / `BIT_MASK(8)`.
 * This is a wire format contract; do not change it without a migration.
 *
 * On success, the event buffer is cleared: the LoRaWAN payload does not carry
 * events, so they are dropped as consumed.
 *
 * @param buf Buffer to append to.
 */
int ctr_power_lrw_encode(struct ctr_buf *buf);

/**
 * @brief Encode the current external power data into a LoRaWAN payload.
 *
 * Appends 5 bytes: `line_voltage` (u16, little-endian, millivolts),
 * `backup_voltage` (u16, little-endian, millivolts), line presence (u8, 1 if
 * `line == CTR_POWER_LINE_CONNECTED`, 0 otherwise). Unknown voltages are encoded
 * as `BIT_MASK(16)`. The third byte is a boolean, not `enum ctr_power_line` --
 * emitting the raw enum would make `CTR_POWER_LINE_DISCONNECTED` (1) decode as
 * connected in every existing decoder, which reads it as `value != 0`.
 * This is a wire format contract; do not change it without a migration.
 *
 * On success, the event buffer is cleared, as with `ctr_power_lrw_encode()`.
 *
 * @param buf Buffer to append to.
 */
int ctr_power_lrw_encode_external(struct ctr_buf *buf);

#endif /* defined(CONFIG_CTR_POWER_LRW) */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_POWER_H_ */
