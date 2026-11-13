/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_modbus.h"
#include "app_config.h"
#include "app_data.h"
#include "drivers/drv_interface.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>
/* Internal Modbus API: needed to raise the RTU frame timeout (no public setter)
 * to work around the SC16IS7xx I2C-UART bridge RX batching, see app_modbus_init,
 * and for the frames of the OR-WE-504 address change, see addr_transact. */
#include <modbus_internal.h>
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

/* Bus ownership (recursive for the owning thread) and enable reference count */
static K_MUTEX_DEFINE(m_bus_mutex);
#if HAS_MODBUS_DEVICE
static int m_enable_count;
#endif

/* Set only once modbus_init_client() has succeeded.
 *
 * Without this, app_modbus_enable()/disable() would happily run PM RESUME/SUSPEND on the
 * SC16IS740 even when app_modbus_init() was never called - and that chip is shared with
 * app_serial. A Modbus driver sampling on a UART-owned bus would then suspend the bridge
 * out from under the UART device, which is far harder to diagnose than a clean -ENODEV.
 */
static bool m_initialized = false;

/* Device-specific sampling functions */

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
	LOG_ERR("Modbus serial device not available in device tree");
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
				.stop_bits =
					(enum uart_config_stop_bits)g_app_config.serial_stop_bits,
			},
	};

	ret = modbus_init_client(m_iface, param);
	if (ret) {
		LOG_ERR("modbus_init_client failed: %d", ret);
		return ret;
	}

	/* Must be set before the enable/disable pair below, which now checks it. */
	m_initialized = true;

	/* The CHESTER X2 RS-485 front-end is an SC16IS7xx I2C-to-UART bridge that
	 * delivers RX bytes in batched bursts with inter-batch gaps larger than the
	 * standard 3.5-character Modbus-RTU end-of-frame silence (~2 ms at 19200
	 * baud). That makes the RTU frame timer expire mid-frame and truncates
	 * multi-byte responses (CRC / frame-length errors). There is no public API
	 * for the RTU frame timeout, so raise it via the Modbus internal context. */
	{
		struct modbus_context *ctx = modbus_get_context(m_iface);

		if (ctx != NULL && ctx->cfg != NULL && ctx->cfg->rtu_timeout < 50000) {
			LOG_INF("Raising RTU frame timeout %u -> 50000 us (SC16IS7xx bridge)",
				ctx->cfg->rtu_timeout);
			ctx->cfg->rtu_timeout = 50000;
		}
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

bool app_modbus_is_ready(void)
{
	return m_initialized;
}

#if HAS_MODBUS_DEVICE
static int power_up(void)
{
	int ret;

	if (!m_initialized) {
		LOG_ERR("Modbus not initialized - refusing to touch the shared UART bridge");
		return -ENODEV;
	}

	if (m_serial_dev == NULL) {
		const struct device *dev = DEVICE_DT_GET(MODBUS_DEVICE_NODE);
		if (!device_is_ready(dev)) {
			LOG_ERR("Modbus device (SC16IS740) not ready - chip absent or "
				"wrong shield/firmware combination?");
			return -ENODEV;
		}
		m_serial_dev = dev;
	}

	ret = pm_device_action_run(m_serial_dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("pm_device_action_run (RESUME) failed: %d", ret);
		return ret;
	}

	LOG_DBG("Modbus interface enabled (%s)", SERIAL_IS_RS485 ? "RS-485" : "RS-232");
	return 0;
}

static int power_down(void)
{
	int ret;

	if (!m_initialized || m_serial_dev == NULL || !device_is_ready(m_serial_dev)) {
		return 0;
	}

	ret = pm_device_action_run(m_serial_dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("pm_device_action_run (SUSPEND) failed: %d", ret);
		return ret;
	}

	LOG_DBG("Modbus interface disabled");
	return 0;
}
#endif /* HAS_MODBUS_DEVICE */

int app_modbus_enable(void)
{
#if HAS_MODBUS_DEVICE
	int ret = 0;

	k_mutex_lock(&m_bus_mutex, K_FOREVER);

	if (m_enable_count == 0) {
		ret = power_up();
	}

	if (!ret) {
		m_enable_count++;
	}

	k_mutex_unlock(&m_bus_mutex);

	return ret;
#else
	LOG_ERR("No serial device (X2/X12) available in device tree");
	return -ENOTSUP;
#endif
}

int app_modbus_disable(void)
{
#if HAS_MODBUS_DEVICE
	int ret = 0;

	k_mutex_lock(&m_bus_mutex, K_FOREVER);

	if (m_enable_count > 0 && --m_enable_count == 0) {
		ret = power_down();
	}

	k_mutex_unlock(&m_bus_mutex);

	return ret;
#else
	LOG_ERR("No serial device (X2/X12) available in device tree");
	return -ENOTSUP;
#endif
}

int app_modbus_acquire(void)
{
	int ret;

	k_mutex_lock(&m_bus_mutex, K_FOREVER);

	ret = app_modbus_enable();
	if (ret) {
		k_mutex_unlock(&m_bus_mutex);
		return ret;
	}

	return 0;
}

void app_modbus_release(void)
{
	app_modbus_disable();

	k_mutex_unlock(&m_bus_mutex);
}

int app_modbus_read_holding_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				 uint16_t *data)
{
	if (data == NULL || count == 0) {
		return -EINVAL;
	}

	int ret = modbus_read_holding_regs(m_iface, slave_addr, reg_addr, data, count);
	if (ret) {
		LOG_WRN("Read holding 0x%04x x%u from slave %u failed: %d", reg_addr, count,
			slave_addr, ret);
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
		LOG_WRN("Read input 0x%04x x%u from slave %u failed: %d", reg_addr, count,
			slave_addr, ret);
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

/* Without the logging wrappers: a timeout at a free address and the -EIO of a reply from
 * the new address are expected during an address change */
static int addr_read_reg(uint8_t addr, uint16_t reg, uint16_t *value, void *user)
{
	ARG_UNUSED(user);

	return modbus_read_holding_regs(m_iface, addr, reg, value, 1);
}

static int addr_write_reg(uint8_t addr, uint16_t reg, uint16_t value, void *user)
{
	ARG_UNUSED(user);

	return modbus_write_holding_reg(m_iface, addr, reg, value);
}

/* The OR-WE-504 unlocks with a proprietary function code and answers the address write from
 * its new address; the client functions reject both replies. The public raw call,
 * modbus_raw_backend_txn(), needs the raw ADU interfaces (CONFIG_MODBUS_RAW_ADU), so do what
 * it does: send the frame and take the reply as it comes. */
static int addr_transact(const struct app_modbus_addr_frame *req, struct app_modbus_addr_frame *rsp,
			 void *user)
{
	struct modbus_context *ctx = modbus_get_context(m_iface);
	int ret;

	ARG_UNUSED(user);

	if (ctx == NULL) {
		return -ENODEV;
	}

	k_mutex_lock(&ctx->iface_lock, K_FOREVER);

	ctx->tx_adu.unit_id = req->unit_id;
	ctx->tx_adu.fc = req->fc;
	ctx->tx_adu.length = req->len;
	memcpy(ctx->tx_adu.data, req->data, req->len);

	ret = modbus_tx_wait_rx_adu(ctx);
	if (!ret && ctx->rx_adu.length > sizeof(rsp->data)) {
		ret = -EMSGSIZE;
	} else if (!ret) {
		rsp->unit_id = ctx->rx_adu.unit_id;
		rsp->fc = ctx->rx_adu.fc;
		rsp->len = ctx->rx_adu.length;
		memcpy(rsp->data, ctx->rx_adu.data, ctx->rx_adu.length);
	}

	k_mutex_unlock(&ctx->iface_lock);

	return ret;
}

static void addr_sleep(int32_t ms, void *user)
{
	ARG_UNUSED(user);

	k_msleep(ms);
}

static const struct app_modbus_addr_bus m_addr_bus = {
	.read_reg = addr_read_reg,
	.write_reg = addr_write_reg,
	.transact = addr_transact,
	.sleep = addr_sleep,
};

int app_modbus_change_addr(uint8_t slave_addr, uint8_t new_addr, uint16_t reg_addr,
			   enum app_modbus_addr_result *result)
{
	int ret;

	ret = app_modbus_acquire();
	if (ret) {
		return ret;
	}

	ret = app_modbus_addr_change(&m_addr_bus, slave_addr, new_addr, reg_addr, result);

	app_modbus_release();

	return ret;
}

int app_modbus_change_addr_or_we_504(uint8_t slave_addr, uint8_t new_addr)
{
	int ret;

	ret = app_modbus_acquire();
	if (ret) {
		return ret;
	}

	/* Held through the unlock window, so that no other request gets into it */
	ret = app_modbus_addr_change_or_we_504(&m_addr_bus, slave_addr, new_addr);

	app_modbus_release();

	return ret;
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

int app_modbus_read_regs(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			 uint16_t count, uint16_t *data)
{
	if (fc == APP_MODBUS_FC_INPUT) {
		return app_modbus_read_input_regs(slave_addr, reg_addr, count, data);
	}

	return app_modbus_read_holding_regs(slave_addr, reg_addr, count, data);
}

int app_modbus_read_u16(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc, uint16_t *out)
{
	return app_modbus_read_regs(slave_addr, reg_addr, fc, 1, out);
}

int app_modbus_read_s16(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc, int16_t *out)
{
	uint16_t reg;
	int ret = app_modbus_read_regs(slave_addr, reg_addr, fc, 1, &reg);
	if (ret) {
		return ret;
	}

	*out = (int16_t)reg;
	return 0;
}

int app_modbus_read_u32(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			enum app_modbus_wo wo, uint32_t *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_regs(slave_addr, reg_addr, fc, 2, regs);
	if (ret) {
		return ret;
	}

	*out = app_modbus_decode_u32(regs, wo);
	return 0;
}

int app_modbus_read_s32(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			enum app_modbus_wo wo, int32_t *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_regs(slave_addr, reg_addr, fc, 2, regs);
	if (ret) {
		return ret;
	}

	*out = app_modbus_decode_s32(regs, wo);
	return 0;
}

int app_modbus_read_float(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			  enum app_modbus_wo wo, float *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_regs(slave_addr, reg_addr, fc, 2, regs);
	if (ret) {
		return ret;
	}

	*out = app_modbus_decode_float(regs, wo);
	return 0;
}

int app_modbus_read_value(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			  enum app_modbus_format fmt, float *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_regs(slave_addr, reg_addr, fc, app_modbus_format_regs(fmt), regs);
	if (ret) {
		return ret;
	}

	*out = app_modbus_decode_value(regs, fmt);
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

/* Configured device without a device driver (handled by sample_device() below) */
static bool is_legacy_device(int dev_idx)
{
	enum app_device_type type = g_app_config.devices[dev_idx].type;

	return type != APP_DEVICE_TYPE_NONE && app_device_find_driver(type) == NULL;
}

static bool has_legacy_device(void)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (is_legacy_device(i)) {
			return true;
		}
	}

	return false;
}

int app_modbus_sample(void)
{
	int ret;
	int errors = 0;
	int sampled = 0;

	/* Devices with a driver are sampled by app_device_sample_all() */
	if (!has_legacy_device()) {
		return 0;
	}

	/* Take the bus (returns -ENOTSUP if no shield) */
	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("app_modbus_acquire failed: %d (sample skipped)", ret);
		return ret;
	}

	/* Sample all configured devices */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (is_legacy_device(i)) {
			ret = sample_device(i);
			if (ret) {
				LOG_WRN("Device[%d] sampling failed: %d", i + 1, ret);
				errors++;
			} else {
				sampled++;
			}
		}
	}

	app_modbus_release();

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

	ret = app_modbus_acquire();
	if (ret == -ENOTSUP) {
		shell_error(shell, "Modbus hardware not available in this build");
		return ret;
	} else if (ret) {
		shell_error(shell, "Failed to enable Modbus: %d", ret);
		return ret;
	}

	if (use_holding) {
		ret = app_modbus_read_holding_regs(slave_addr, reg_addr, count, data);
	} else {
		ret = app_modbus_read_input_regs(slave_addr, reg_addr, count, data);
	}

	app_modbus_release();

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

	int ret = app_modbus_acquire();
	if (ret == -ENOTSUP) {
		shell_error(shell, "Modbus hardware not available in this build");
		return ret;
	} else if (ret) {
		shell_error(shell, "Failed to enable Modbus: %d", ret);
		return ret;
	}

	ret = app_modbus_write_holding_reg(slave_addr, reg_addr, value);

	app_modbus_release();

	if (ret) {
		shell_error(shell, "Write failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Successfully wrote 0x%04x to register 0x%04x at slave %d", value,
		    reg_addr, slave_addr);

	return 0;
}

static int print_change_addr_error(const struct shell *shell, int ret, uint8_t slave_addr,
				   uint8_t new_addr)
{
	switch (ret) {
	case -EINVAL:
		shell_error(shell, "The device already has address %u", new_addr);
		break;
	case -EADDRINUSE:
		shell_error(shell, "Address %u is already used on the bus", new_addr);
		break;
	case -ETIMEDOUT:
		shell_error(shell, "No reply from address %u", slave_addr);
		break;
	case -EACCES:
		shell_error(shell, "The device refused the address change");
		break;
	case -ENODEV:
		shell_error(shell, "Modbus is not running: set serial-mode modbus and a device-N, "
				   "then app config save");
		break;
	case -ENOTSUP:
		shell_error(shell, "Modbus hardware not available in this build");
		break;
	default:
		shell_error(shell, "Address change failed: %d", ret);
		break;
	}

	return ret;
}

int app_modbus_shell_change_addr(const struct shell *shell, uint8_t slave_addr, uint8_t new_addr,
				 uint16_t reg_addr)
{
	enum app_modbus_addr_result result;
	int ret;

	/* Up to a few reply timeouts when the device does not answer */
	shell_print(shell, "Changing address %u to %u...", slave_addr, new_addr);

	ret = app_modbus_change_addr(slave_addr, new_addr, reg_addr, &result);
	if (ret) {
		return print_change_addr_error(shell, ret, slave_addr, new_addr);
	}

	if (result == APP_MODBUS_ADDR_PENDING) {
		shell_print(shell, "Address %u stored, power-cycle the device to apply it",
			    new_addr);
	} else {
		shell_print(shell, "Address changed from %u to %u", slave_addr, new_addr);
	}

	return 0;
}

int app_modbus_shell_change_addr_or_we_504(const struct shell *shell, uint8_t slave_addr,
					   uint8_t new_addr)
{
	int ret;

	/* The write goes 8 s after the unlock */
	shell_print(shell, "Unlocking address %u and changing it to %u (about 10 s)...", slave_addr,
		    new_addr);

	ret = app_modbus_change_addr_or_we_504(slave_addr, new_addr);
	if (ret) {
		return print_change_addr_error(shell, ret, slave_addr, new_addr);
	}

	shell_print(shell, "Address changed from %u to %u", slave_addr, new_addr);

	return 0;
}

int cmd_modbus_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (!has_legacy_device()) {
		shell_print(shell, "No device without a driver configured (lambrecht, generic)");
		shell_print(shell, "Use 'sample' for all devices or 'device sample <idx>' for one");
		return 0;
	}

	shell_print(shell, "Sampling devices without a driver...");

	int ret = app_modbus_sample();
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Sampling complete");
	return 0;
}
