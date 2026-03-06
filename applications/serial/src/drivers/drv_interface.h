/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_INTERFACE_H_
#define DRV_INTERFACE_H_

/**
 * @file drv_interface.h
 * @brief Driver interface for device drivers
 *
 * This header is included only by device drivers (drv_microsens_180hs.c, etc.).
 * It defines the interface that each driver must implement.
 */

#include "../app_device.h"
#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device driver interface
 *
 * Each device driver implements this interface.
 * Functions can be NULL if the driver doesn't support that operation.
 *
 * Example usage in a driver:
 *
 * @code
 * static const struct app_device_driver my_driver = {
 *     .name = "mydevice",
 *     .type = APP_DEVICE_TYPE_MYDEVICE,
 *     .init = mydevice_init,
 *     .sample = mydevice_sample,
 *     .deinit = NULL,                   // Optional
 *     .calibrate = mydevice_calibrate,  // Optional
 *     .reset = mydevice_reset,          // Optional
 * };
 * @endcode
 */
struct app_device_driver {
	/* Identification */
	const char *name;          /**< Driver name ("microsens", "sensecap", ...) */
	enum app_device_type type; /**< Device type enum value */

	/* Required functions */
	int (*init)(void);   /**< Initialize hardware (UART, I2C, etc.) */
	int (*sample)(void); /**< Perform measurement, store in internal structure */

	/* Optional functions (NULL = not supported) */
	int (*deinit)(void); /**< Deinitialize (power down) */
	int (*calibrate)(enum app_device_calibration cal_type,
			 int value);   /**< Calibrate sensor */
	int (*reset)(void);            /**< Factory reset */
	int (*config)(uint8_t new_addr,
		      const char *parity_str); /**< Configure device (Modbus addr, parity) */

	void (*print_data)(const struct shell *shell,
			   int idx, int addr); /**< Print last measurement data to shell */
};

/**
 * @brief Get driver instance by type
 *
 * Used internally by app_device.c to find the appropriate driver.
 *
 * @param type Device type to look up
 * @return Pointer to driver structure, or NULL if not found
 */
const struct app_device_driver *app_device_find_driver(enum app_device_type type);

/*
 * Driver declarations
 *
 * Each driver declares its driver structure here so app_device.c can reference it.
 * Add new driver declarations when implementing new device types.
 */

/* MicroSens 180-HS driver */
extern const struct app_device_driver microsens_driver;

/* Energy meter drivers */
extern const struct app_device_driver or_we_504_driver;
extern const struct app_device_driver em1xx_driver;
extern const struct app_device_driver or_we_516_driver;
extern const struct app_device_driver em5xx_driver;
extern const struct app_device_driver iem3000_driver;

/* Promag MF7S RFID reader driver */
extern const struct app_device_driver promag_mf7s_driver;

/* FlowT FT201 ultrasonic flowmeter driver */
extern const struct app_device_driver flowt_ft201_driver;

/* Shell command entries for energy meters */
extern const union shell_cmd_entry or_we_504_shell_cmds;
extern const union shell_cmd_entry em1xx_shell_cmds;
extern const union shell_cmd_entry or_we_516_shell_cmds;
extern const union shell_cmd_entry em5xx_shell_cmds;
extern const union shell_cmd_entry iem3000_shell_cmds;
extern const union shell_cmd_entry promag_mf7s_shell_cmds;
extern const union shell_cmd_entry flowt_ft201_shell_cmds;

#ifdef __cplusplus
}
#endif

#endif /* DRV_INTERFACE_H_ */
