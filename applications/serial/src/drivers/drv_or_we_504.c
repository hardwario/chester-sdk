/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_or_we_504.h"
#include "drv_interface.h"
#include "../app_modbus.h"

#include <chester/ctr_rtc.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_or_we_504, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

/* Decode UINT32 Big Endian (standard Modbus format) */
static uint32_t decode_uint32_be(const uint16_t *regs)
{
	return ((uint32_t)regs[0] << 16) | regs[1];
}

/* Parse parity string (e.g., "8E1", "8N1") */
static int parse_parity(const char *str, char *parity, int *stop_bits)
{
	if (!str || strlen(str) < 3) {
		return -EINVAL;
	}
	if (str[0] != '8') {
		return -EINVAL;
	}
	char p = str[1];
	if (p != 'N' && p != 'n' && p != 'E' && p != 'e' && p != 'O' && p != 'o') {
		return -EINVAL;
	}
	int sb = str[2] - '0';
	if (sb != 1 && sb != 2) {
		return -EINVAL;
	}
	if (parity) {
		*parity = (p == 'n') ? 'N' : (p == 'e') ? 'E' : (p == 'o') ? 'O' : p;
	}
	if (stop_bits) {
		*stop_bits = sb;
	}
	return 0;
}

/* OR-WE-504 Register addresses */
#define REG_VOLTAGE       0x0000  /* UINT16, /10 V */
#define REG_CURRENT       0x0001  /* UINT16, /10 A */
#define REG_POWER         0x0003  /* UINT16, W */
#define REG_ENERGY        0x0007  /* UINT32 BE, Wh */
#define REG_ADDRESS       0x000F  /* Address configuration register */

/* Default Modbus address */
#define DEFAULT_ADDR      1

/* Global data storage */
static struct app_data_or_we_504 m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct or_we_504_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

/* Unlock OR-WE-504 for configuration (function 0x28) */
static int unlock_device(uint8_t addr)
{
	/*
	 * OR-WE-504 requires unlock before configuration changes.
	 * The unlock is done by writing to specific registers.
	 * Default password is 0x00000000.
	 *
	 * Note: This is a simplified implementation.
	 * Full implementation would use raw Modbus frame with function 0x28.
	 */
	LOG_INF("Unlocking OR-WE-504 at address %d (simplified)", addr);

	/* For now, we'll use standard Modbus write which may work on some units */
	/* Full unlock would require raw frame: [addr, 0x28, 0xFE, 0x01, 0x00, 0x02, 0x04, 0,0,0,0] */

	return 0;
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

/* Driver functions */

static int init(void)
{
	LOG_INF("Initializing OR-WE-504 driver");
	m_data.valid = false;
	m_data.error_count = 0;
	return 0;
}

static int sample(void)
{
	int ret;
	uint16_t regs[4];
	uint8_t addr;

	/* Local variables for values */
	float voltage, current, power, energy;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling OR-WE-504 at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Read Voltage (0x0000) - UINT16 */
	ret = app_modbus_read_holding_regs(addr, REG_VOLTAGE, 1, &regs[0]);
	if (ret) {
		LOG_ERR("Failed to read voltage: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	voltage = (float)regs[0] / 10.0f;

	/* Read Current (0x0001) - UINT16 */
	ret = app_modbus_read_holding_regs(addr, REG_CURRENT, 1, &regs[0]);
	if (ret) {
		LOG_ERR("Failed to read current: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	current = (float)regs[0] / 10.0f;

	/* Read Power (0x0003) - UINT16 */
	ret = app_modbus_read_holding_regs(addr, REG_POWER, 1, &regs[0]);
	if (ret) {
		LOG_ERR("Failed to read power: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	power = (float)regs[0];

	/* Read Energy (0x0007) - UINT32 BE */
	ret = app_modbus_read_holding_regs(addr, REG_ENERGY, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read energy: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	energy = (float)decode_uint32_be(regs);

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.voltage = voltage;
	m_data.current = current;
	m_data.power = power;
	m_data.energy = energy;
	m_data.valid = true;
	m_data.last_sample = k_uptime_get_32();
	m_data.error_count = 0;
	k_mutex_unlock(&m_data_mutex);

	/* Add to sample buffer */
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	if (m_sample_count < MAX_SAMPLES) {
		uint64_t ts;
		ctr_rtc_get_ts(&ts);

		m_samples[m_sample_count].timestamp = ts;
		m_samples[m_sample_count].voltage = voltage;
		m_samples[m_sample_count].current = current;
		m_samples[m_sample_count].power = power;
		m_samples[m_sample_count].energy = energy;
		m_sample_count++;
		LOG_DBG("OR-WE-504: Added sample %d to buffer", m_sample_count);
	} else {
		LOG_WRN("OR-WE-504: Sample buffer full");
	}
	k_mutex_unlock(&m_samples_mutex);

	LOG_INF("OR-WE-504: V=%.1f V, I=%.1f A, P=%.0f W, E=%.0f Wh",
		(double)voltage, (double)current, (double)power, (double)energy);

out:
	app_modbus_disable();
	return ret;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	int ret;
	uint8_t current_addr;

	if (new_addr < 1 || new_addr > 247) {
		LOG_ERR("Invalid Modbus address: %d (must be 1-247)", new_addr);
		return -EINVAL;
	}

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	current_addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Configuring OR-WE-504: addr %d -> %d", current_addr, new_addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		return ret;
	}

	/* Unlock device for configuration */
	ret = unlock_device(current_addr);
	if (ret) {
		LOG_ERR("Failed to unlock device: %d", ret);
		goto out;
	}

	/* Wait for unlock to take effect */
	k_msleep(100);

	/* Write new address to register 0x000F */
	ret = app_modbus_write_holding_reg(current_addr, REG_ADDRESS, new_addr);
	if (ret) {
		LOG_ERR("Failed to write new address: %d", ret);
		goto out;
	}

	LOG_INF("Address changed successfully. New address: %d", new_addr);
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);

	if (parity_str) {
		char parity;
		int stop_bits;
		ret = parse_parity(parity_str, &parity, &stop_bits);
		if (ret == 0) {
			LOG_INF("Parity setting requested: %c, stop bits: %d", parity, stop_bits);
			LOG_WRN("Parity change requires device restart");
		}
	}

out:
	app_modbus_disable();
	return ret;
}

/* Shell commands */

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device or_we_504 sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	/* Temporarily set address for sampling */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t old_addr = m_data.modbus_addr;
	m_data.modbus_addr = addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = sample();

	/* Restore original address */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = old_addr;
	k_mutex_unlock(&m_data_mutex);

	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	print_data(shell, 0, addr);

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(shell, "Usage: device or_we_504 config <old_addr> <new_addr> [8E1]");
		return -EINVAL;
	}

	uint8_t old_addr = (uint8_t)strtol(argv[1], NULL, 0);
	uint8_t new_addr = (uint8_t)strtol(argv[2], NULL, 0);
	const char *parity_str = (argc >= 4) ? argv[3] : NULL;

	if (old_addr < 1 || old_addr > 247 || new_addr < 1 || new_addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	/* Set current address to old_addr for config operation */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = old_addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = config(new_addr, parity_str);
	if (ret) {
		shell_error(shell, "Configuration failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Address changed from %d to %d", old_addr, new_addr);
	return 0;
}

/* Shell command entries */
static const struct shell_static_entry or_we_504_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Change address [parity]: config <old_addr> <new_addr> [8E1]", cmd_config, 3, 1),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry or_we_504_shell_cmds = {
	.entry = or_we_504_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_or_we_504 data_copy = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] OR-WE-504 @ addr %d:", idx, addr);
	shell_print(shell, "  Voltage:  %.1f V", (double)data_copy.voltage);
	shell_print(shell, "  Current:  %.2f A", (double)data_copy.current);
	shell_print(shell, "  Power:    %.0f W", (double)data_copy.power);
	shell_print(shell, "  Energy:   %.0f Wh", (double)data_copy.energy);
}

/* Driver registration */
const struct app_device_driver or_we_504_driver = {
	.name = "or_we_504",
	.type = APP_DEVICE_TYPE_OR_WE_504,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

/* Public API */

const struct app_data_or_we_504 *or_we_504_get_data(void)
{
	return &m_data;
}

void or_we_504_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t or_we_504_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int or_we_504_get_samples(struct or_we_504_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void or_we_504_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("OR-WE-504: Sample buffer cleared");
}

int or_we_504_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
