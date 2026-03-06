/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file app_device.c
 * @brief Device manager implementation
 *
 * This module provides a unified interface for all device drivers.
 * It handles driver registration, device lookup, and API dispatch.
 */

#include "app_device.h"
#include "app_config.h"
#include "app_data.h"
#include "drivers/drv_interface.h"
#include "drivers/drv_microsens_180hs.h"
#include "drivers/drv_or_we_504.h"
#include "drivers/drv_em1xx.h"
#include "drivers/drv_or_we_516.h"
#include "drivers/drv_em5xx.h"
#include "drivers/drv_iem3000.h"
#include "drivers/drv_promag_mf7s.h"
#include "drivers/drv_flowt_ft201.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_device, LOG_LEVEL_INF);

/* Device type names for string conversion */
static const char *const m_type_names[] = {
	[APP_DEVICE_TYPE_NONE] = "none",
	[APP_DEVICE_TYPE_MICROSENS_180HS] = "microsens_180hs",
	[APP_DEVICE_TYPE_SENSECAP_S1000] = "sensecap_s1000",
	[APP_DEVICE_TYPE_CUBIC_6303] = "cubic_6303",
	[APP_DEVICE_TYPE_LAMBRECHT] = "lambrecht",
	[APP_DEVICE_TYPE_GENERIC] = "generic",
	/* Energy meters */
	[APP_DEVICE_TYPE_OR_WE_504] = "or_we_504",
	[APP_DEVICE_TYPE_EM1XX] = "em1xx",
	[APP_DEVICE_TYPE_OR_WE_516] = "or_we_516",
	[APP_DEVICE_TYPE_EM5XX] = "em5xx",
	[APP_DEVICE_TYPE_IEM3000] = "iem3000",
	[APP_DEVICE_TYPE_PROMAG_MF7S] = "promag_mf7s",
	[APP_DEVICE_TYPE_FLOWT_FT201] = "flowt_ft201",
};

const struct app_device_driver *app_device_find_driver(enum app_device_type type)
{
	switch (type) {
	case APP_DEVICE_TYPE_MICROSENS_180HS:
		return &microsens_driver;
	case APP_DEVICE_TYPE_OR_WE_504:
		return &or_we_504_driver;
	case APP_DEVICE_TYPE_EM1XX:
		return &em1xx_driver;
	case APP_DEVICE_TYPE_OR_WE_516:
		return &or_we_516_driver;
	case APP_DEVICE_TYPE_EM5XX:
		return &em5xx_driver;
	case APP_DEVICE_TYPE_IEM3000:
		return &iem3000_driver;
	case APP_DEVICE_TYPE_PROMAG_MF7S:
		return &promag_mf7s_driver;
	case APP_DEVICE_TYPE_FLOWT_FT201:
		return &flowt_ft201_driver;
	default:
		return NULL;
	}
}

int app_device_init(void)
{
	int ret;
	int init_count = 0;
	int error_count = 0;

	LOG_INF("Initializing device drivers");

	/* Initialize drivers for all configured devices */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = g_app_config.devices[i].type;
		if (type == APP_DEVICE_TYPE_NONE) {
			continue;
		}

		const struct app_device_driver *drv = app_device_find_driver(type);

		if (drv == NULL) {
			LOG_WRN("No driver for device[%d] type %d", i, type);
			continue;
		}

		if (drv->init == NULL) {
			LOG_WRN("Driver '%s' has no init function", drv->name);
			continue;
		}

		LOG_INF("Initializing device[%d] driver '%s'", i, drv->name);

		ret = drv->init();
		if (ret) {
			LOG_ERR("Failed to init '%s': %d", drv->name, ret);
			error_count++;
		} else {
			LOG_INF("Initialized '%s' driver", drv->name);
			init_count++;
		}
	}

	if (init_count == 0 && error_count == 0) {
		LOG_INF("No devices configured");
		return 0;
	}

	LOG_INF("Device init complete: %d initialized, %d errors", init_count, error_count);

	return error_count > 0 ? -EIO : 0;
}

int app_device_sample(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		LOG_ERR("Invalid device index: %d", device_idx);
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[device_idx].type;
	if (type == APP_DEVICE_TYPE_NONE) {
		LOG_DBG("Device[%d] not configured", device_idx);
		return -ENODEV;
	}

	const struct app_device_driver *drv = app_device_find_driver(type);

	if (drv == NULL || drv->sample == NULL) {
		LOG_ERR("No sample function for device[%d] type %d", device_idx, type);
		return -ENOTSUP;
	}

	/* Set the configured Modbus address before sampling */
	uint8_t addr = g_app_config.devices[device_idx].addr;
	switch (type) {
	case APP_DEVICE_TYPE_OR_WE_504:
		or_we_504_set_addr(addr);
		break;
	case APP_DEVICE_TYPE_EM1XX:
		em1xx_set_addr(addr);
		break;
	case APP_DEVICE_TYPE_OR_WE_516:
		or_we_516_set_addr(addr);
		break;
	case APP_DEVICE_TYPE_EM5XX:
		em5xx_set_addr(addr);
		break;
	case APP_DEVICE_TYPE_IEM3000:
		iem3000_set_addr(addr);
		break;
	case APP_DEVICE_TYPE_FLOWT_FT201:
		flowt_ft201_set_addr(addr);
		break;
	default:
		/* RS232 devices (MicroSens) don't need address */
		break;
	}

	LOG_DBG("Sampling device[%d] (%s) at addr %d", device_idx, drv->name, addr);

	return drv->sample();
}

int app_device_sample_all(void)
{
	int ret;
	int errors = 0;
	int sampled = 0;

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_CONFIG_DEVICE_TYPE_NONE) {
			ret = app_device_sample(i);
			if (ret) {
				LOG_WRN("Device[%d] sampling failed: %d", i, ret);
				errors++;
			} else {
				sampled++;
			}

			/* Small delay between devices for RS-485 bus settling
			 * This gives the bus time to stabilize after enable/disable cycles
			 * and helps prevent state corruption between consecutive devices.
			 */
			k_sleep(K_MSEC(50));
		}
	}

	if (sampled == 0) {
		LOG_DBG("No devices configured for sampling");
		return -ENODATA;
	}

	return errors > 0 ? -EIO : 0;
}

int app_device_calibrate(int device_idx, enum app_device_calibration cal_type, int value)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[device_idx].type;
	if (type == APP_DEVICE_TYPE_NONE) {
		return -ENODEV;
	}

	const struct app_device_driver *drv = app_device_find_driver(type);

	if (drv == NULL || drv->calibrate == NULL) {
		LOG_WRN("Calibration not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Calibrating device[%d] (%s): cal_type=%d, value=%d", device_idx, drv->name,
		cal_type, value);

	return drv->calibrate(cal_type, value);
}

int app_device_reset(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[device_idx].type;
	if (type == APP_DEVICE_TYPE_NONE) {
		return -ENODEV;
	}

	const struct app_device_driver *drv = app_device_find_driver(type);

	if (drv == NULL || drv->reset == NULL) {
		LOG_WRN("Reset not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Resetting device[%d] (%s)", device_idx, drv->name);

	return drv->reset();
}

const char *app_device_type_name(enum app_device_type type)
{
	if (type >= 0 && type < ARRAY_SIZE(m_type_names) && m_type_names[type] != NULL) {
		return m_type_names[type];
	}
	return "unknown";
}

enum app_device_type app_device_type_from_name(const char *name)
{
	if (name == NULL) {
		return APP_DEVICE_TYPE_NONE;
	}

	for (int i = 0; i < ARRAY_SIZE(m_type_names); i++) {
		if (m_type_names[i] != NULL && strcmp(m_type_names[i], name) == 0) {
			return (enum app_device_type)i;
		}
	}

	/* Also try short names */
	if (strcmp(name, "microsens") == 0) {
		return APP_DEVICE_TYPE_MICROSENS_180HS;
	}
	if (strcmp(name, "sensecap") == 0) {
		return APP_DEVICE_TYPE_SENSECAP_S1000;
	}
	if (strcmp(name, "cubic") == 0) {
		return APP_DEVICE_TYPE_CUBIC_6303;
	}
	/* Energy meter short names */
	if (strcmp(name, "em111") == 0) {
		return APP_DEVICE_TYPE_EM1XX;
	}
	if (strcmp(name, "em540") == 0) {
		return APP_DEVICE_TYPE_EM5XX;
	}
	if (strcmp(name, "promag") == 0) {
		return APP_DEVICE_TYPE_PROMAG_MF7S;
	}
	if (strcmp(name, "flowt") == 0) {
		return APP_DEVICE_TYPE_FLOWT_FT201;
	}

	return APP_DEVICE_TYPE_NONE;
}

bool app_device_is_configured(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return false;
	}

	return g_app_config.devices[device_idx].type != APP_CONFIG_DEVICE_TYPE_NONE;
}

const struct app_data_microsens *app_device_get_microsens_data(void)
{
	return microsens_get_data();
}

int app_device_config(int device_idx, uint8_t new_addr, const char *parity_str)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[device_idx].type;
	if (type == APP_DEVICE_TYPE_NONE) {
		return -ENODEV;
	}

	const struct app_device_driver *drv = app_device_find_driver(type);

	if (drv == NULL || drv->config == NULL) {
		LOG_WRN("Config not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Configuring device[%d] (%s): new_addr=%d", device_idx, drv->name, new_addr);

	return drv->config(new_addr, parity_str);
}

/* Energy meter data access */
const struct app_data_or_we_504 *app_device_get_or_we_504_data(void)
{
	return or_we_504_get_data();
}

const struct app_data_em1xx *app_device_get_em1xx_data(void)
{
	return em1xx_get_data();
}

const struct app_data_or_we_516 *app_device_get_or_we_516_data(void)
{
	return or_we_516_get_data();
}

const struct app_data_em5xx *app_device_get_em5xx_data(void)
{
	return em5xx_get_data();
}

const struct app_data_iem3000 *app_device_get_iem3000_data(void)
{
	return iem3000_get_data();
}

const struct app_data_promag_mf7s *app_device_get_promag_mf7s_data(void)
{
	return promag_mf7s_get_data();
}

const struct app_data_flowt_ft201 *app_device_get_flowt_ft201_data(void)
{
	return flowt_ft201_get_data();
}

/*
 * Shell commands
 */

/* Device list command */

static int cmd_device_list(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "Configured devices:");

	bool found = false;
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = g_app_config.devices[i].type;
		if (type != APP_DEVICE_TYPE_NONE) {
			const char *type_name = app_device_type_name(type);

			shell_print(shell, "  [%d] %s (addr=%d, timeout=%ds)", i, type_name,
				    g_app_config.devices[i].addr,
				    g_app_config.devices[i].timeout_s);
			found = true;
		}
	}

	if (!found) {
		shell_print(shell, "  (none)");
	}

	return 0;
}

/* Sample device by index (from config) */
static int cmd_device_sample_idx(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device sample <idx>");
		shell_print(shell, "  idx = 0-%d (from device-0..device-7 config)",
			    APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	int idx = strtol(argv[1], NULL, 0);
	if (idx < 0 || idx >= APP_CONFIG_MAX_DEVICES) {
		shell_error(shell, "Invalid index (0-%d)", APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[idx].type;

	if (type == APP_DEVICE_TYPE_NONE) {
		shell_error(shell, "Device[%d] not configured", idx);
		return -ENODEV;
	}

	int ret = app_device_sample(idx);
	if (ret) {
		shell_error(shell, "Sampling device[%d] failed: %d", idx, ret);
		return ret;
	}

	/* Print results via driver's print_data function */
	uint8_t addr = g_app_config.devices[idx].addr;
	const struct app_device_driver *drv = app_device_find_driver(type);

	if (drv && drv->print_data) {
		drv->print_data(shell, idx, addr);
	} else {
		const char *type_name = app_device_type_name(type);
		shell_print(shell, "[%d] %s @ addr %d: OK", idx, type_name, addr);
	}

	return 0;
}

/* Reset device by index */
static int cmd_device_reset_idx(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device reset <idx>");
		return -EINVAL;
	}

	int idx = strtol(argv[1], NULL, 0);
	if (idx < 0 || idx >= APP_CONFIG_MAX_DEVICES) {
		shell_error(shell, "Invalid index (0-%d)", APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	int ret = app_device_reset(idx);
	if (ret == -ENOTSUP) {
		shell_error(shell, "Reset not supported for device[%d]", idx);
		return ret;
	}
	if (ret == -ENODEV) {
		shell_error(shell, "Device[%d] not configured", idx);
		return ret;
	}
	if (ret) {
		shell_error(shell, "Reset device[%d] failed: %d", idx, ret);
		return ret;
	}

	shell_print(shell, "Device[%d] reset complete", idx);
	return 0;
}

/* clang-format off */

/* Device root command
 *
 * Each device driver exports its shell subcommands (e.g., microsens_shell_cmds from microsens_180hs.h).
 * When adding a new device driver:
 * 1. Create shell commands in the driver file (e.g., sensecap.c)
 * 2. Export subcommand set in driver header (e.g., sensecap_shell_cmds)
 * 3. Add SHELL_CMD entry here referencing the exported subcommands
 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_device,
	/* Device-specific commands (with address parameter) */
	SHELL_CMD(microsens_180hs, &microsens_shell_cmds, "MicroSENS 180-HS CO2 sensor commands", NULL),
	SHELL_CMD(or_we_504, &or_we_504_shell_cmds, "Orno OR-WE-504 single-phase meter", NULL),
	SHELL_CMD(em1xx, &em1xx_shell_cmds, "Carlo Gavazzi EM1XX (EM111) single-phase meter", NULL),
	SHELL_CMD(or_we_516, &or_we_516_shell_cmds, "Orno OR-WE-516 3-phase meter", NULL),
	SHELL_CMD(em5xx, &em5xx_shell_cmds, "Carlo Gavazzi EM5XX (EM540) 3-phase meter", NULL),
	SHELL_CMD(iem3000, &iem3000_shell_cmds, "Schneider iEM3000 3-phase meter", NULL),
	SHELL_CMD(promag_mf7s, &promag_mf7s_shell_cmds, "Promag MF7S RFID reader", NULL),
	SHELL_CMD(flowt_ft201, &flowt_ft201_shell_cmds, "FlowT FT201 ultrasonic flowmeter", NULL),
	/* Generic commands (use device index from config) */
	SHELL_CMD_ARG(sample, NULL, "Sample device by index: sample <0-7>", cmd_device_sample_idx, 2, 0),
	SHELL_CMD_ARG(reset, NULL, "Reset device by index: reset <0-7>", cmd_device_reset_idx, 2, 0),
	SHELL_CMD_ARG(list, NULL, "List configured devices", cmd_device_list, 1, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(device, &sub_device, "Device commands", NULL);

/* clang-format on */
