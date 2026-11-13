/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_PIKETRONIC_RPPU_H_
#define DRV_PIKETRONIC_RPPU_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Latest decoded reading from the RPP-U probe.
 *
 * Field names follow the vendor's "Actual data get - D" table.
 */
struct drv_piketronic_rppu_data {
	bool valid;

	uint32_t real_time;            /**< seconds since 2000-01-01 (probe RTC) */
	int8_t temperature;            /**< degC in measuring chamber */
	uint8_t humidity;              /**< % RH in measuring chamber */
	uint32_t concentration_a;      /**< Bq/m3, 0.5 h avg from Po-218 (faster, less sensitive) */
	uint32_t concentration_ac;     /**< Bq/m3, 0.5 h avg Po-218 + Po-214 (slower, more sensitive) */
	uint32_t concentration_day_ac; /**< Bq/m3, 1 day avg */
	uint16_t concentration_time;   /**< s into the 120 s update cycle */
	uint32_t sum1;                 /**< alpha counts below d1 (service) */
	uint32_t sum2;                 /**< alpha counts d1..d2 = Po-218 (service) */
	uint32_t sum3;                 /**< alpha counts d2..d3 = Po-214 (service) */
	uint32_t sum4;                 /**< alpha counts above d3 (service) */
	uint8_t impulses_hv;           /**< HV generator status (service) */
	uint16_t record_time;          /**< s into the record-saving interval */
	uint32_t record_count;         /**< records stored in probe memory */
	uint32_t spectrum_time;        /**< s into the spectrum-saving interval */
	uint16_t spectrum_count;       /**< spectra stored */
	uint32_t impulses_total;       /**< lifetime alpha count (service) */
	uint8_t switch_state;          /**< top ON/OFF switch: 0 or 1 */
	uint16_t voltage_mv;           /**< accumulator mV; probe powers off below 3300 */
	uint8_t errors;                /**< 0x00 = normal operation */

	uint32_t read_ok;   /**< successful reads since boot */
	uint32_t read_fail; /**< failed reads since boot */
};

/**
 * @brief Sample entry for the CBOR/LRW uplink (lean subset of the full reading)
 *
 * Slot-for-slot the same shape as the RPP-R sample - the sensitive 0.5 h average
 * (Po-218 + Po-214) and the 1 day average, then temperature and humidity - plus this
 * probe's own accumulator voltage, which the bus-powered RPP-R has no equivalent for.
 * Note the first field is a 0.5 h average here; the RPP-R's is a 1 h average.
 */
struct piketronic_rppu_sample {
	uint64_t timestamp;             /**< system RTC timestamp */
	uint32_t concentration;         /**< Bq/m3, 0.5 h avg (concentration_ac) */
	uint32_t concentration_day;     /**< Bq/m3, 1 day avg (concentration_day_ac) */
	int8_t temperature;             /**< degC */
	uint8_t humidity;               /**< % RH */
	uint16_t voltage_mv;            /**< accumulator mV; probe powers off below 3300 */
};

struct app_driver_slot;

/**
 * @brief Latest reading of a slot (caller holds slot->lock)
 *
 * The samples for the uplink are the records of the slot (struct piketronic_rppu_sample).
 */
const struct drv_piketronic_rppu_data *piketronic_rppu_get_data(const struct app_driver_slot *slot);

/**
 * @brief Read the 10-character serial number (V command) into @p buf.
 *
 * @param buf Destination, at least 11 bytes
 * @param size Size of @p buf
 * @return 0 on success, negative errno otherwise
 */
int drv_piketronic_rppu_read_serial(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DRV_PIKETRONIC_RPPU_H_ */
