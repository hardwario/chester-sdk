/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_PIKETRONIC_RPP_H_
#define DRV_PIKETRONIC_RPP_H_

/**
 * @file drv_piketronic_rpp.h
 * @brief Piketronic RPP-R radon probe driver (Modbus RTU over RS485)
 *
 * Reads radon concentration, temperature and humidity from the Piketronic
 * RPP-R probe. Default communication: 19200 baud, 8E1, slave address 1
 * (set by DIP switches on the probe itself).
 *
 * Register map (function 0x03, register number == Modbus address on this
 * device - no "minus one" offset):
 *   1      concentrationTime  (UINT, seconds into the 4-min interval)
 *   2-3    concentration      (UINT32, Bq/m3, 1-hour moving average)
 *   4      temperature        (signed int8 in low byte, degC)
 *   5      humidity           (UINT, %)
 *   17-18  concentrationDay   (UINT32, Bq/m3, 1-day moving average)
 *   33     limit              (UINT, alarm threshold)              [writable]
 *   36     RecordInterval     (UINT, minutes)                     [writable]
 *   37     SpectrumInterval   (UINT, minutes)                     [writable]
 *   38     algorithm          (0=RnA, 1..255=RnA+RnC)             [writable]
 *   60-74  device/version/serial (10 ASCII chars each)
 *
 * Calibration registers 28-35 are intentionally NOT writable through this
 * driver (writing them corrupts the probe's factory calibration).
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Piketronic RPP-R data structure */
struct app_data_piketronic_rpp {
	/* Measurements */
	uint32_t concentration;      /* Bq/m3, 1-hour moving average (reg 2-3) */
	uint32_t concentration_day;  /* Bq/m3, 1-day moving average (reg 17-18) */
	int8_t temperature;          /* degC (reg 4) */
	uint8_t humidity;            /* % (reg 5) */
	uint16_t concentration_time; /* seconds into 4-min interval (reg 1) */

	/* Settings */
	uint16_t limit;              /* alarm threshold (reg 33) */
	uint16_t record_interval;    /* minutes (reg 36) */
	uint16_t spectrum_interval;  /* minutes (reg 37) */
	uint16_t algorithm;          /* 0=RnA, 1..255=RnA+RnC (reg 38) */

	/* Identification */
	char device[11];             /* reg 60-64 */
	char version_sw[11];         /* reg 65-69 */
	char serial_number[11];      /* reg 70-74 */

	/* Bookkeeping */
	uint8_t modbus_addr;
	uint32_t last_sample;
	uint32_t error_count;
	bool valid;
};

/* Sample entry for CBOR buffer (lean uplink payload) */
struct piketronic_rpp_sample {
	uint64_t timestamp;
	uint32_t concentration;     /* Bq/m3, 1-hour average */
	uint32_t concentration_day; /* Bq/m3, 1-day average */
	int8_t temperature;         /* degC */
	uint8_t humidity;           /* % */
};

const struct app_data_piketronic_rpp *piketronic_rpp_get_data(void);
void piketronic_rpp_set_addr(uint8_t addr);
uint8_t piketronic_rpp_get_addr(void);

int piketronic_rpp_get_samples(struct piketronic_rpp_sample *out, int max_count);
void piketronic_rpp_clear_samples(void);
int piketronic_rpp_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_PIKETRONIC_RPP_H_ */
