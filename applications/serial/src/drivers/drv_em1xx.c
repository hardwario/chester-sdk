/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_em1xx.h"
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

LOG_MODULE_REGISTER(drv_em1xx, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

/* Decode INT32 Low Word First (Carlo Gavazzi format) */
static int32_t decode_int32_lsw(const uint16_t *regs)
{
	uint32_t value = ((uint32_t)regs[1] << 16) | regs[0];
	return (int32_t)value;
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

/* EM1XX Register addresses (INT32 LSW) */
#define REG_CURRENT       0x0100  /* /1000 A */
#define REG_VOLTAGE       0x0102  /* /10 V */
#define REG_POWER         0x0106  /* /10000 kW */
#define REG_FREQUENCY     0x0110  /* /10 Hz */
#define REG_ENERGY_IN     0x0112  /* /10 kWh */
#define REG_ENERGY_OUT    0x0116  /* /10 kWh */

#define DEFAULT_ADDR      1

static struct app_data_em1xx m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct em1xx_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing EM1XX driver");
	m_data.valid = false;
	m_data.error_count = 0;
	return 0;
}

static int sample(void)
{
	int ret;
	uint16_t regs[2];
	int32_t val;
	uint8_t addr;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling EM1XX at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Local variables for values */
	float current, voltage, power, frequency, energy_in, energy_out;

	/* Read Current (0x0100) */
	ret = app_modbus_read_holding_regs(addr, REG_CURRENT, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read current: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	current = (float)val / 1000.0f;

	/* Read Voltage (0x0102) */
	ret = app_modbus_read_holding_regs(addr, REG_VOLTAGE, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read voltage: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	voltage = (float)val / 10.0f;

	/* Read Power (0x0106) */
	ret = app_modbus_read_holding_regs(addr, REG_POWER, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read power: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	power = (float)val / 10000.0f;

	/* Read Frequency (0x0110) */
	ret = app_modbus_read_holding_regs(addr, REG_FREQUENCY, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read frequency: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	frequency = (float)val / 10.0f;

	/* Read Energy IN (0x0112) */
	ret = app_modbus_read_holding_regs(addr, REG_ENERGY_IN, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read energy_in: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	energy_in = (float)val / 10.0f;

	/* Read Energy OUT (0x0116) */
	ret = app_modbus_read_holding_regs(addr, REG_ENERGY_OUT, 2, regs);
	if (ret) {
		LOG_ERR("Failed to read energy_out: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		m_data.valid = false;
		k_mutex_unlock(&m_data_mutex);
		goto out;
	}
	val = decode_int32_lsw(regs);
	energy_out = (float)val / 10.0f;

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.current = current;
	m_data.voltage = voltage;
	m_data.power = power;
	m_data.frequency = frequency;
	m_data.energy_in = energy_in;
	m_data.energy_out = energy_out;
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
		m_samples[m_sample_count].frequency = frequency;
		m_samples[m_sample_count].energy_in = energy_in;
		m_samples[m_sample_count].energy_out = energy_out;
		m_sample_count++;
		LOG_DBG("EM1XX: Added sample %d to buffer", m_sample_count);
	} else {
		LOG_WRN("EM1XX: Sample buffer full");
	}
	k_mutex_unlock(&m_samples_mutex);

	LOG_INF("EM1XX: V=%.1f V, I=%.3f A, P=%.4f kW, F=%.1f Hz, E_in=%.1f kWh, E_out=%.1f kWh",
		(double)voltage, (double)current, (double)power,
		(double)frequency, (double)energy_in, (double)energy_out);

out:
	app_modbus_disable();
	return ret;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	if (new_addr < 1 || new_addr > 247) {
		LOG_ERR("Invalid Modbus address: %d", new_addr);
		return -EINVAL;
	}

	LOG_INF("EM1XX config: addr change not implemented via Modbus");
	LOG_INF("Use device configuration software or set address parameter");

	if (parity_str) {
		char parity;
		int stop_bits;
		int ret = parse_parity(parity_str, &parity, &stop_bits);
		if (ret == 0) {
			LOG_INF("Parity: %c, stop bits: %d (requires device restart)", parity, stop_bits);
		}
	}

	/* Update local address for sampling */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);
	LOG_INF("Sampling address set to %d", new_addr);

	return 0;
}

/* Shell commands */

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device em1xx sample <addr>");
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
		shell_error(shell, "Usage: device em1xx config <old_addr> <new_addr> [8E1]");
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

static const struct shell_static_entry em1xx_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Change address [parity]: config <old_addr> <new_addr> [8E1]", cmd_config, 3, 1),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry em1xx_shell_cmds = {
	.entry = em1xx_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_em1xx data_copy = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] EM1XX (EM111) @ addr %d:", idx, addr);
	shell_print(shell, "  Voltage:    %.1f V", (double)data_copy.voltage);
	shell_print(shell, "  Current:    %.3f A", (double)data_copy.current);
	shell_print(shell, "  Power:      %.4f kW", (double)data_copy.power);
	shell_print(shell, "  Frequency:  %.1f Hz", (double)data_copy.frequency);
	shell_print(shell, "  Energy IN:  %.1f kWh", (double)data_copy.energy_in);
	shell_print(shell, "  Energy OUT: %.1f kWh", (double)data_copy.energy_out);
}

const struct app_device_driver em1xx_driver = {
	.name = "em1xx",
	.type = APP_DEVICE_TYPE_EM1XX,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_em1xx *em1xx_get_data(void)
{
	return &m_data;
}

void em1xx_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t em1xx_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int em1xx_get_samples(struct em1xx_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void em1xx_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("EM1XX: Sample buffer cleared");
}

int em1xx_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
