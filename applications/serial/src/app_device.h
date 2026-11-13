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
struct app_config_device;
struct app_driver_slot;

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
	APP_DEVICE_TYPE_PIKETRONIC_RPP = 13, /* Piketronic RPP-R radon probe */
	APP_DEVICE_TYPE_SOLAX_G3 = 14,    /* SolaX X3-Hybrid G3 inverter */
	APP_DEVICE_TYPE_PIKETRONIC_RPPU = 15, /* Piketronic RPP-U radon probe (UART variant) */
	APP_DEVICE_TYPE_SENSECAP_S500 = 16, /* Seeed SenseCAP S500 weather station */
	APP_DEVICE_TYPE_SENSECAP_S700 = 17, /* Seeed SenseCAP S700 weather station */
	APP_DEVICE_TYPE_COUNT, /* Number of types */
};

/**
 * @brief Transport a device driver talks over
 *
 * A device is reached either through the Modbus RTU client (app_modbus_*, RS-485
 * multi-drop) or as a raw byte stream on the UART (app_serial_*, point-to-point).
 * This is a property of the device itself, not of how the user configured the
 * application - which is why it lives on the driver.
 *
 * Value 0 is MODBUS so a driver that forgets to state its transport gets the
 * conservative default instead of landing on the raw-UART path. Types with no
 * driver at all (lambrecht, generic - handled by the switch inside app_modbus.c)
 * never read this field; app_device_get_transport() resolves them explicitly.
 */
enum app_device_transport {
	APP_DEVICE_TRANSPORT_MODBUS = 0, /**< via app_modbus_* (RS-485/RS-232 RTU) */
	APP_DEVICE_TRANSPORT_UART,       /**< via app_serial_* (raw byte stream) */
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
 * @brief Pick the transport of the serial bus for a device configuration
 *
 * The SC16IS7xx of the X2 / X12 slot runs either the raw UART (app_serial) or the Modbus
 * client, not both: they install the same IRQ callback and the last one wins. Every configured
 * device needs one transport, the one of its driver; the driverless legacy types use Modbus,
 * and the UART in transparent mode. When all devices need the same transport, the bus runs it,
 * otherwise serial-mode decides: modbus selects Modbus, uart and transparent select UART.
 * app_device_init() does not activate a device that needs the other transport.
 *
 * @param devices Device configuration (APP_CONFIG_MAX_DEVICES entries)
 * @param serial_mode Configured serial-mode (enum app_config_serial_mode)
 * @param bus Transport of the bus
 * @return true if a device is configured, so the bus is needed
 */
bool app_device_pick_bus(const struct app_config_device *devices, int serial_mode,
			 enum app_device_transport *bus);

/**
 * @brief Initialize device manager
 *
 * Plans the device slots and initializes the drivers of the configured devices. A device
 * whose transport differs from the one of the bus (app_device_pick_bus()), or a second device
 * on a UART bus, is not activated. Call it after the bus is initialized.
 *
 * @return 0 on success, negative error code on failure
 */
int app_device_init(void);

/**
 * @brief Check a device configuration before it is saved
 *
 * Checks the per-type instance limits of the drivers and that the base device buffers fit
 * the shared memory pool. Rings are capped, so it warns where a device keeps less than
 * one report period of data.
 *
 * @param shell Shell to print the reason of a rejection to
 * @param devices Parsed device-0..7 configuration (APP_CONFIG_MAX_DEVICES entries)
 * @param interval_sample Sample interval in seconds
 * @param interval_aggreg Aggregation interval in seconds
 * @param interval_report Report interval in seconds (0 = on demand)
 * @param mode Communication mode (enum app_config_mode)
 * @return 0 if the configuration is usable, negative error code otherwise
 */
int app_device_check_config(const struct shell *shell, const struct app_config_device *devices,
			    int interval_sample, int interval_aggreg, int interval_report,
			    int mode);

/**
 * @brief Get the slot of a configured device
 *
 * @param device_idx Device index (0-based)
 * @return Slot, or NULL if the device is not configured or has no active driver
 */
struct app_driver_slot *app_device_get_slot(int device_idx);

/**
 * @brief Close the aggregation window of every table-driven device (interval-aggreg)
 *
 * @param ticks Expirations of the aggregation timer since the previous call (more than one
 *              if the work queue was blocked longer than an interval)
 */
void app_device_aggreg_all(uint32_t ticks);

/**
 * @brief Drop the buffered records and windows of all devices (after an upload)
 */
void app_device_clear_all(void);

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
 * @brief Get the transport a device type talks over
 *
 * Looks the type up in the driver table and returns the driver's declared
 * transport. Types without a driver fall back to APP_DEVICE_TRANSPORT_MODBUS,
 * matching the legacy in-app_modbus.c sampling path.
 *
 * @param type Device type enum value
 * @return Transport used by that device type
 */
enum app_device_transport app_device_get_transport(enum app_device_type type);

/**
 * @brief Check whether a device type has a driver
 *
 * Types without a driver (lambrecht, generic) are served by the legacy switch inside
 * app_modbus.c and have no transport of their own - for those the configured
 * serial-mode decides. Callers need to tell that case apart from a real driver that
 * declares APP_DEVICE_TRANSPORT_MODBUS.
 *
 * @param type Device type enum value
 * @return true if a driver is registered for this type
 */
bool app_device_has_driver(enum app_device_type type);

/**
 * @brief Check if a device slot is configured
 *
 * @param device_idx Device index (0-based)
 * @return true if device is configured, false otherwise
 */
bool app_device_is_configured(int device_idx);

/**
 * @brief Configure a device (change Modbus address, parity)
 *
 * @param device_idx Device index (0-based)
 * @param new_addr New Modbus address (1-247)
 * @param parity_str Parity string (e.g., "8E1", "8N1") or NULL for current setting
 * @return 0 on success, -ENOTSUP if not supported, negative error on failure
 */
int app_device_config(int device_idx, uint8_t new_addr, const char *parity_str);

#ifdef __cplusplus
}
#endif

#endif /* APP_DEVICE_H_ */
