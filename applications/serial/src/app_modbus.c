/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_modbus.h"
#include "app_config.h"
#include "app_data.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <math.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_modbus, LOG_LEVEL_DBG);

/* Check which serial device is available for Modbus */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x2_sc16is740_a), okay)
#define HAS_MODBUS_DEVICE    1
#define MODBUS_DEVICE_NODE   DT_NODELABEL(ctr_x2_sc16is740_a)
#define SERIAL_IS_RS485      1
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x12_sc16is740_a), okay)
#define HAS_MODBUS_DEVICE    1
#define MODBUS_DEVICE_NODE   DT_NODELABEL(ctr_x12_sc16is740_a)
#define SERIAL_IS_RS485      0
#else
#define HAS_MODBUS_DEVICE    0
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_modbus_serial)
#define HAS_MODBUS_SERIAL 1
#else
#define HAS_MODBUS_SERIAL 0
#endif

static int m_iface = 0;
#if HAS_MODBUS_DEVICE
static const struct device *m_serial_dev = NULL;
#endif

/* Helper functions for reading different data types */

static int read_reg_ui16(uint8_t slave_addr, uint16_t reg_addr, uint16_t *data)
{
	uint16_t reg_data;
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, &reg_data, 1);
	if (ret) {
		LOG_WRN("Read reg 0x%04x from slave %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	*data = reg_data;
	return 0;
}

__maybe_unused static int read_holding_reg_ui16(uint8_t slave_addr, uint16_t reg_addr, uint16_t *data)
{
	uint16_t reg_data;
	int ret = modbus_read_holding_regs(m_iface, slave_addr, reg_addr, &reg_data, 1);
	if (ret) {
		LOG_WRN("Read holding reg 0x%04x from slave %d failed: %d", reg_addr, slave_addr,
			ret);
		return ret;
	}

	*data = reg_data;
	return 0;
}

__maybe_unused static int write_reg_ui16(uint8_t slave_addr, uint16_t reg_addr, uint16_t data)
{
	int ret = modbus_write_holding_reg(m_iface, slave_addr, reg_addr, data);
	if (ret) {
		LOG_WRN("Write reg 0x%04x to slave %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	LOG_INF("Successfully wrote 0x%04x to reg 0x%04x at slave %d", data, reg_addr, slave_addr);
	return 0;
}

static int read_reg_s32(uint8_t slave_addr, uint16_t reg_addr, int32_t *data)
{
	uint16_t reg_data[2];
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, reg_data, 2);
	if (ret) {
		LOG_WRN("Read reg 0x%04x (32-bit) from slave %d failed: %d", reg_addr, slave_addr,
			ret);
		return ret;
	}

	*data = (int32_t)reg_data[0] << 16 | reg_data[1];
	return 0;
}

static int read_reg_u32(uint8_t slave_addr, uint16_t reg_addr, uint32_t *data)
{
	uint16_t reg_data[2];
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, reg_data, 2);
	if (ret) {
		LOG_WRN("Read reg 0x%04x (32-bit) from slave %d failed: %d", reg_addr, slave_addr,
			ret);
		return ret;
	}

	*data = (uint32_t)reg_data[0] << 16 | reg_data[1];
	return 0;
}

/* Device-specific sampling functions */

static int modbus_sample_sensecap_s1000(uint8_t slave_addr)
{
	int ret;
	int32_t modbus_data_s32;
	const float divisor = 1000.0f;

	LOG_INF("Sampling SenseCAP S1000 at slave address %d", slave_addr);

	/* Air temperature (0x0000) */
	ret = read_reg_s32(slave_addr, 0x0000, &modbus_data_s32);
	if (ret == 0) {
		float temperature = (float)modbus_data_s32 / divisor;
		LOG_INF("  Temperature: %.2f C", (double)temperature);
	}

	/* Air humidity (0x0002) */
	ret = read_reg_s32(slave_addr, 0x0002, &modbus_data_s32);
	if (ret == 0) {
		float humidity = (float)modbus_data_s32 / divisor;
		LOG_INF("  Humidity: %.2f %%", (double)humidity);
	}

	/* Barometric pressure (0x0004) */
	ret = read_reg_s32(slave_addr, 0x0004, &modbus_data_s32);
	if (ret == 0) {
		float pressure = (float)modbus_data_s32 / divisor;
		LOG_INF("  Pressure: %.2f Pa", (double)pressure);
	}

	/* Light intensity (0x0006) */
	ret = read_reg_s32(slave_addr, 0x0006, &modbus_data_s32);
	if (ret == 0) {
		float light = (float)modbus_data_s32 / divisor;
		LOG_INF("  Light: %.2f lux", (double)light);
	}

	/* Wind direction avg (0x000C) */
	ret = read_reg_s32(slave_addr, 0x000C, &modbus_data_s32);
	if (ret == 0) {
		float wind_dir = (float)modbus_data_s32 / divisor;
		LOG_INF("  Wind Direction: %.2f deg", (double)wind_dir);
	}

	/* Wind speed avg (0x0012) */
	ret = read_reg_s32(slave_addr, 0x0012, &modbus_data_s32);
	if (ret == 0) {
		float wind_speed = (float)modbus_data_s32 / divisor;
		LOG_INF("  Wind Speed: %.2f m/s", (double)wind_speed);
	}

	return 0;
}

static int modbus_sample_cubic_6303(uint8_t slave_addr)
{
	int ret;
	uint32_t modbus_data_u32;
	uint16_t modbus_data_u16;

	LOG_INF("Sampling Cubic 6303 PM sensor at slave address %d", slave_addr);

	/* Version (IR1, addr 0) */
	ret = read_reg_ui16(slave_addr, 0, &modbus_data_u16);
	if (ret == 0) {
		float version = (float)modbus_data_u16 / 100.0f;
		LOG_INF("  Version: %.2f", (double)version);
	}

	/* PM1.0 (IR8/9, addr 7) */
	ret = read_reg_u32(slave_addr, 7, &modbus_data_u32);
	if (ret == 0) {
		LOG_INF("  PM1.0: %u ug/m3", modbus_data_u32);
	}

	/* PM2.5 (IR10/11, addr 9) */
	ret = read_reg_u32(slave_addr, 9, &modbus_data_u32);
	if (ret == 0) {
		LOG_INF("  PM2.5: %u ug/m3", modbus_data_u32);
	}

	/* PM10 (IR14/15, addr 13) */
	ret = read_reg_u32(slave_addr, 13, &modbus_data_u32);
	if (ret == 0) {
		LOG_INF("  PM10: %u ug/m3", modbus_data_u32);
	}

	return 0;
}

static int modbus_sample_lambrecht(uint8_t slave_addr)
{
	LOG_INF("Sampling Lambrecht meteo station at slave address %d", slave_addr);
	LOG_WRN("Lambrecht sampling not yet implemented");
	/* TODO: Implement Lambrecht-specific sampling */
	return -ENOSYS;
}

static int modbus_sample_generic(uint8_t slave_addr)
{
	int ret;
	uint16_t reg_data[10];

	LOG_INF("Sampling generic Modbus device at slave address %d", slave_addr);

	/* Read first 10 input registers as a demo */
	ret = modbus_read_input_regs(m_iface, slave_addr, 0, reg_data, 10);
	if (ret) {
		LOG_ERR("Failed to read input registers: %d", ret);
		return ret;
	}

	LOG_INF("Read 10 input registers:");
	LOG_HEXDUMP_INF(reg_data, sizeof(reg_data), "Registers");

	return 0;
}

/* Public API */

int app_modbus_init(void)
{
#if !HAS_MODBUS_SERIAL
	LOG_WRN("Modbus serial device not available in device tree");
	return -ENOTSUP;
#else
	int ret;

	LOG_INF("Initializing Modbus RTU client");

	/* Get Modbus interface by device tree name */
	m_iface = modbus_iface_get_by_name(
		DEVICE_DT_NAME(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)));
	if (m_iface < 0) {
		LOG_ERR("Modbus interface not found. Ensure Modbus is enabled in device tree");
		return m_iface;
	}

	/* Find maximum timeout among all configured devices
	 * CRITICAL FIX: Previously took timeout from FIRST device only, causing timeout
	 * issues for devices with many operations (like OR-WE-516 with 19 reads) when
	 * they weren't first. Now uses maximum timeout to accommodate all devices.
	 */
	int default_timeout_ms = 1000; /* Fallback default: 1 second */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_CONFIG_DEVICE_TYPE_NONE) {
			int dev_timeout_ms = g_app_config.devices[i].timeout_s * 1000;
			/* Use this device's timeout if it's higher than current max
			 * (skip if timeout_s is 0, which means use default)
			 */
			if (dev_timeout_ms > 0 && dev_timeout_ms > default_timeout_ms) {
				default_timeout_ms = dev_timeout_ms;
			}
		}
	}

	/* Configure Modbus parameters from app_config */
	const struct modbus_iface_param param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = default_timeout_ms * 1000, /* Convert ms to us */
		.serial =
			{
				.baud = g_app_config.serial_baudrate,
				.parity = (enum uart_config_parity)g_app_config.serial_parity,
				.stop_bits_client =
					(enum uart_config_stop_bits)g_app_config.serial_stop_bits,
			},
	};

	ret = modbus_init_client(m_iface, param);
	if (ret) {
		LOG_ERR("modbus_init_client failed: %d", ret);
		return ret;
	}

	/* Enable Modbus device */
	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("app_modbus_enable failed: %d", ret);
		return ret;
	}

	LOG_INF("Modbus RTU client initialized on interface %d", m_iface);
	LOG_INF("  Baud rate: %d", g_app_config.serial_baudrate);
	LOG_INF("  Parity: %d", g_app_config.serial_parity);
	LOG_INF("  Stop bits: %d", g_app_config.serial_stop_bits);
	LOG_INF("  Timeout: %d ms", default_timeout_ms);

	/* Disable after initialization (power saving) */
	ret = app_modbus_disable();
	if (ret) {
		LOG_ERR("app_modbus_disable failed: %d", ret);
		return ret;
	}

	return 0;
#endif
}

int app_modbus_enable(void)
{
#if HAS_MODBUS_DEVICE
	int ret;

	if (m_serial_dev == NULL) {
		m_serial_dev = DEVICE_DT_GET(MODBUS_DEVICE_NODE);
		if (!device_is_ready(m_serial_dev)) {
			LOG_ERR("Modbus device (SC16IS740) not ready");
			return -ENODEV;
		}
	}

	ret = pm_device_action_run(m_serial_dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("pm_device_action_run (RESUME) failed: %d", ret);
		return ret;
	}

	LOG_DBG("Modbus interface enabled (%s)", SERIAL_IS_RS485 ? "RS-485" : "RS-232");
	return 0;
#else
	LOG_WRN("No serial device (X2/X12) available in device tree");
	return -ENOTSUP;
#endif
}

int app_modbus_disable(void)
{
#if HAS_MODBUS_DEVICE
	int ret;

	if (m_serial_dev == NULL || !device_is_ready(m_serial_dev)) {
		return 0;
	}

	ret = pm_device_action_run(m_serial_dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("pm_device_action_run (SUSPEND) failed: %d", ret);
		return ret;
	}

	LOG_DBG("Modbus interface disabled");
	return 0;
#else
	LOG_WRN("No serial device (X2/X12) available in device tree");
	return -ENOTSUP;
#endif
}

int app_modbus_read_holding_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				 uint16_t *data)
{
	if (data == NULL || count == 0) {
		return -EINVAL;
	}

	int ret = modbus_read_holding_regs(m_iface, slave_addr, reg_addr, data, count);
	if (ret) {
		LOG_WRN("modbus_read_holding_regs failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_modbus_read_input_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
			       uint16_t *data)
{
	if (data == NULL || count == 0) {
		return -EINVAL;
	}

	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, data, count);
	if (ret) {
		LOG_WRN("modbus_read_input_regs failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_modbus_write_holding_reg(uint8_t slave_addr, uint16_t reg_addr, uint16_t data)
{
	int ret = modbus_write_holding_reg(m_iface, slave_addr, reg_addr, data);
	if (ret) {
		LOG_WRN("modbus_write_holding_reg failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_modbus_write_holding_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				  const uint16_t *data)
{
	if (data == NULL || count == 0) {
		return -EINVAL;
	}

	int ret = modbus_write_holding_regs(m_iface, slave_addr, reg_addr, (uint16_t *)data, count);
	if (ret) {
		LOG_WRN("modbus_write_holding_regs failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_modbus_read_coils(uint8_t slave_addr, uint16_t coil_addr, uint16_t count, uint8_t *data)
{
	if (data == NULL || count == 0) {
		return -EINVAL;
	}

	int ret = modbus_read_coils(m_iface, slave_addr, coil_addr, data, count);
	if (ret) {
		LOG_WRN("modbus_read_coils failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_modbus_write_coil(uint8_t slave_addr, uint16_t coil_addr, bool value)
{
	int ret = modbus_write_coil(m_iface, slave_addr, coil_addr, value);
	if (ret) {
		LOG_WRN("modbus_write_coil failed: %d", ret);
		return ret;
	}

	return 0;
}

/* Sample a single device by index */
static int sample_device(int dev_idx)
{
	int ret;
	const struct app_config_device *dev = &g_app_config.devices[dev_idx];
	uint8_t slave_addr = (uint8_t)dev->addr;

	if (dev->type == APP_CONFIG_DEVICE_TYPE_NONE) {
		return 0; /* Skip unconfigured devices */
	}

	LOG_INF("Sampling device[%d]: addr=%d, timeout=%ds", dev_idx, slave_addr, dev->timeout_s);

	/* Sample based on configured device type */
	switch (dev->type) {
	case APP_CONFIG_DEVICE_TYPE_SENSECAP_S1000:
		ret = modbus_sample_sensecap_s1000(slave_addr);
		break;

	case APP_CONFIG_DEVICE_TYPE_CUBIC_6303:
		ret = modbus_sample_cubic_6303(slave_addr);
		break;

	case APP_CONFIG_DEVICE_TYPE_LAMBRECHT:
		ret = modbus_sample_lambrecht(slave_addr);
		break;

	case APP_CONFIG_DEVICE_TYPE_GENERIC:
		ret = modbus_sample_generic(slave_addr);
		break;

	case APP_CONFIG_DEVICE_TYPE_MICROSENS_180HS:
		LOG_WRN("Microsens 180HS sampling not yet implemented for Modbus");
		ret = -ENOSYS;
		break;

	case APP_CONFIG_DEVICE_TYPE_NONE:
	default:
		ret = 0;
		break;
	}

	return ret;
}

int app_modbus_sample(void)
{
	int ret;
	int errors = 0;
	int sampled = 0;

	/* Enable Modbus interface */
	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("app_modbus_enable failed: %d", ret);
		return ret;
	}

	/* Sample all configured devices */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_CONFIG_DEVICE_TYPE_NONE) {
			ret = sample_device(i);
			if (ret) {
				LOG_WRN("Device[%d] sampling failed: %d", i + 1, ret);
				errors++;
			} else {
				sampled++;
			}
		}
	}

	/* Disable Modbus interface */
	int disable_ret = app_modbus_disable();
	if (disable_ret) {
		LOG_ERR("app_modbus_disable failed: %d", disable_ret);
		return disable_ret;
	}

	if (sampled == 0) {
		LOG_WRN("No devices configured for sampling");
		return -EINVAL;
	}

	LOG_INF("Sampling complete: %d devices sampled, %d errors", sampled, errors);
	return errors > 0 ? -EIO : 0;
}

/* Shell commands */

int cmd_modbus_read(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 4) {
		shell_error(shell, "Usage: %s <slave_addr> <reg_addr> <count> [holding|input]",
			    argv[0]);
		return -EINVAL;
	}

	uint8_t slave_addr = (uint8_t)strtol(argv[1], NULL, 0);
	uint16_t reg_addr = (uint16_t)strtol(argv[2], NULL, 0);
	uint16_t count = (uint16_t)strtol(argv[3], NULL, 0);

	if (count > 32) {
		shell_error(shell, "Count too large (max 32)");
		return -EINVAL;
	}

	bool use_holding = false;
	if (argc >= 5 && strcmp(argv[4], "holding") == 0) {
		use_holding = true;
	}

	uint16_t data[32];
	int ret;

	app_modbus_enable();

	if (use_holding) {
		ret = app_modbus_read_holding_regs(slave_addr, reg_addr, count, data);
	} else {
		ret = app_modbus_read_input_regs(slave_addr, reg_addr, count, data);
	}

	app_modbus_disable();

	if (ret) {
		shell_error(shell, "Read failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Read %d %s registers from slave %d, starting at 0x%04x:", count,
		    use_holding ? "holding" : "input", slave_addr, reg_addr);
	shell_hexdump(shell, (const uint8_t *)data, count * 2);

	return 0;
}

int cmd_modbus_write(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 4) {
		shell_error(shell, "Usage: %s <slave_addr> <reg_addr> <value>", argv[0]);
		return -EINVAL;
	}

	uint8_t slave_addr = (uint8_t)strtol(argv[1], NULL, 0);
	uint16_t reg_addr = (uint16_t)strtol(argv[2], NULL, 0);
	uint16_t value = (uint16_t)strtol(argv[3], NULL, 0);

	app_modbus_enable();

	int ret = app_modbus_write_holding_reg(slave_addr, reg_addr, value);

	app_modbus_disable();

	if (ret) {
		shell_error(shell, "Write failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Successfully wrote 0x%04x to register 0x%04x at slave %d", value,
		    reg_addr, slave_addr);

	return 0;
}

int cmd_modbus_sample(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "Sampling configured device...");

	int ret = app_modbus_sample();
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Sampling complete");
	return 0;
}
