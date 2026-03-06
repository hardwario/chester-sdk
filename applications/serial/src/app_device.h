/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DEVICE_H_
#define APP_DEVICE_H_

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct shell;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device types - central definition
 *
 * This enum defines all supported device types.
 * Add new device types here when implementing new drivers.
 */
enum app_device_type {
	APP_DEVICE_TYPE_NONE = 0,
	APP_DEVICE_TYPE_SENSECAP_S1000 = 1,
	APP_DEVICE_TYPE_CUBIC_6303 = 2,
	APP_DEVICE_TYPE_LAMBRECHT = 3,
	APP_DEVICE_TYPE_GENERIC = 4,
	APP_DEVICE_TYPE_MICROSENS_180HS = 5,
	/* Energy meters */
	APP_DEVICE_TYPE_OR_WE_504 = 6,  /* Orno OR-WE-504 single-phase */
	APP_DEVICE_TYPE_EM1XX = 7,      /* Carlo Gavazzi EM1XX (EM111) single-phase */
	APP_DEVICE_TYPE_OR_WE_516 = 8,  /* Orno OR-WE-516 3-phase */
	APP_DEVICE_TYPE_EM5XX = 9,      /* Carlo Gavazzi EM5XX (EM540) 3-phase */
	APP_DEVICE_TYPE_IEM3000 = 10,   /* Schneider iEM3000 3-phase */
	APP_DEVICE_TYPE_PROMAG_MF7S = 11, /* Promag MF7S RFID reader */
	APP_DEVICE_TYPE_FLOWT_FT201 = 12, /* FlowT FT201 ultrasonic flowmeter */
	APP_DEVICE_TYPE_COUNT, /* Number of types */
};

/**
 * @brief Calibration types
 */
enum app_device_calibration {
	APP_DEVICE_CAL_ZERO = 0,
	APP_DEVICE_CAL_SPAN = 1,
};

/*
 * Public API - application calls only these functions
 */

/**
 * @brief Initialize device manager
 *
 * Initializes all registered device drivers.
 * Should be called once at application startup.
 *
 * @return 0 on success, negative error code on failure
 */
int app_device_init(void);

/**
 * @brief Sample a specific device
 *
 * Calls the appropriate driver's sample function based on device configuration.
 *
 * @param device_idx Device index (0-based, corresponds to device[1] = 0)
 * @return 0 on success, negative error code on failure
 */
int app_device_sample(int device_idx);

/**
 * @brief Sample all configured devices
 *
 * Iterates through all configured devices and samples each one.
 *
 * @return 0 on success, -ENODATA if no devices configured, -EIO if any failed
 */
int app_device_sample_all(void);

/**
 * @brief Calibrate a device
 *
 * @param device_idx Device index (0-based)
 * @param cal_type Calibration type (zero, span, etc.)
 * @param value Optional calibration value
 * @return 0 on success, -ENOTSUP if not supported, negative error on failure
 */
int app_device_calibrate(int device_idx, enum app_device_calibration cal_type, int value);

/**
 * @brief Reset a device
 *
 * @param device_idx Device index (0-based)
 * @return 0 on success, -ENOTSUP if not supported, negative error on failure
 */
int app_device_reset(int device_idx);

/*
 * Utility functions
 */

/**
 * @brief Get device type name string
 *
 * @param type Device type enum value
 * @return Human-readable device type name
 */
const char *app_device_type_name(enum app_device_type type);

/**
 * @brief Parse device type from name string
 *
 * @param name Device type name string
 * @return Device type enum value, or APP_DEVICE_TYPE_NONE if not found
 */
enum app_device_type app_device_type_from_name(const char *name);

/**
 * @brief Check if a device slot is configured
 *
 * @param device_idx Device index (0-based)
 * @return true if device is configured, false otherwise
 */
bool app_device_is_configured(int device_idx);

/*
 * Device-specific data access
 *
 * Applications can access device-specific data through these functions.
 * Each function returns a pointer to the device's internal data structure.
 */

/* Forward declaration for device-specific data structures */
struct app_data_microsens;
struct app_data_or_we_504;
struct app_data_em1xx;
struct app_data_or_we_516;
struct app_data_em5xx;
struct app_data_iem3000;
struct app_data_promag_mf7s;
struct app_data_flowt_ft201;

/**
 * @brief Get MicroSens 180-HS sensor data
 *
 * @return Pointer to MicroSens data structure, or NULL if not available
 */
const struct app_data_microsens *app_device_get_microsens_data(void);

/**
 * @brief Configure a device (change Modbus address, parity)
 *
 * @param device_idx Device index (0-based)
 * @param new_addr New Modbus address (1-247)
 * @param parity_str Parity string (e.g., "8E1", "8N1") or NULL for current setting
 * @return 0 on success, -ENOTSUP if not supported, negative error on failure
 */
int app_device_config(int device_idx, uint8_t new_addr, const char *parity_str);

/* Energy meter data access */
const struct app_data_or_we_504 *app_device_get_or_we_504_data(void);
const struct app_data_em1xx *app_device_get_em1xx_data(void);
const struct app_data_or_we_516 *app_device_get_or_we_516_data(void);
const struct app_data_em5xx *app_device_get_em5xx_data(void);
const struct app_data_iem3000 *app_device_get_iem3000_data(void);
const struct app_data_promag_mf7s *app_device_get_promag_mf7s_data(void);
const struct app_data_flowt_ft201 *app_device_get_flowt_ft201_data(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DEVICE_H_ */
