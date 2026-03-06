/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

/* ### Preserved code "includes" (begin) */
/* Device types - single enum definition */
#include "app_device.h"

/* Backward compatibility aliases for device types */
#define APP_CONFIG_DEVICE_TYPE_NONE            APP_DEVICE_TYPE_NONE
#define APP_CONFIG_DEVICE_TYPE_SENSECAP_S1000  APP_DEVICE_TYPE_SENSECAP_S1000
#define APP_CONFIG_DEVICE_TYPE_CUBIC_6303      APP_DEVICE_TYPE_CUBIC_6303
#define APP_CONFIG_DEVICE_TYPE_LAMBRECHT       APP_DEVICE_TYPE_LAMBRECHT
#define APP_CONFIG_DEVICE_TYPE_GENERIC         APP_DEVICE_TYPE_GENERIC
#define APP_CONFIG_DEVICE_TYPE_MICROSENS_180HS APP_DEVICE_TYPE_MICROSENS_180HS

#define APP_CONFIG_MAX_DEVICES     8
#define APP_CONFIG_DEVICE_STR_SIZE 32

struct app_config_device {
	enum app_device_type type; /* Device type */
	int addr;                  /* Modbus address (1-247), 0 = not set */
	int timeout_s;             /* Response timeout in seconds, default 1 */
};
/* ^^^ Preserved code "includes" (end) */

#ifdef __cplusplus
extern "C" {
#endif

enum app_config_mode {
	APP_CONFIG_MODE_NONE = 0,
	APP_CONFIG_MODE_LTE = 1,
	APP_CONFIG_MODE_LRW = 2,
};

enum app_config_serial_mode {
	APP_CONFIG_SERIAL_MODE_TRANSPARENT = 0,
	APP_CONFIG_SERIAL_MODE_MODBUS = 1,
};

enum app_config_serial_parity {
	APP_CONFIG_SERIAL_PARITY_NONE = 0,
	APP_CONFIG_SERIAL_PARITY_ODD = 1,
	APP_CONFIG_SERIAL_PARITY_EVEN = 2,
};

struct app_config {
	int interval_sample;
	int interval_aggreg;
	int interval_report;
	int interval_poll;
	enum app_config_serial_mode serial_mode;
	int serial_baudrate;
	int serial_data_bits;
	enum app_config_serial_parity serial_parity;
	int serial_stop_bits;

	enum app_config_mode mode;

	/* ### Preserved code "struct variables" (begin) */
	/* Device config strings (stored in settings) */
	char device_str[APP_CONFIG_MAX_DEVICES][APP_CONFIG_DEVICE_STR_SIZE];

	/* Parsed device config (populated in h_commit) */
	struct app_config_device devices[APP_CONFIG_MAX_DEVICES];
	/* ^^^ Preserved code "struct variables" (end) */
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv);

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv);

/* ### Preserved code "functions" (begin) */
/* ^^^ Preserved code "functions" (end) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
