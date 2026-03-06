/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_em5xx.h"
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

LOG_MODULE_REGISTER(drv_em5xx, LOG_LEVEL_DBG);

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

/* EM5XX Register addresses (INT32 LSW) */
#define REG_CURRENT           0x00F8  /* /1000 A */
#define REG_VOLTAGE           0x0102  /* /10 V */
#define REG_POWER             0x0106  /* /10000 kW */
#define REG_FREQUENCY         0x0110  /* /10 Hz */
#define REG_ENERGY_IN         0x0112  /* /10 kWh */
#define REG_ENERGY_OUT        0x0116  /* /10 kWh */
#define REG_VOLTAGE_L1        0x0120  /* /10 V */
#define REG_CURRENT_L1        0x0122  /* /1000 A */
#define REG_POWER_L1          0x0124  /* /10000 kW */
#define REG_VOLTAGE_L2        0x012E  /* /10 V */
#define REG_CURRENT_L2        0x0130  /* /1000 A */
#define REG_POWER_L2          0x0132  /* /10000 kW */
#define REG_VOLTAGE_L3        0x013C  /* /10 V */
#define REG_CURRENT_L3        0x013E  /* /1000 A */
#define REG_POWER_L3          0x0140  /* /10000 kW */

#define DEFAULT_ADDR          1

static struct app_data_em5xx m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct em5xx_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

static int read_int32_lsw(uint8_t addr, uint16_t reg, float *out, float divisor)
{
	uint16_t regs[2];
	int ret = app_modbus_read_holding_regs(addr, reg, 2, regs);
	if (ret) {
		return ret;
	}
	int32_t val = decode_int32_lsw(regs);
	*out = (float)val / divisor;
	return 0;
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing EM5XX driver");
	m_data.valid = false;
	m_data.error_count = 0;
	return 0;
}

static int sample(void)
{
	int ret;
	int errors = 0;
	uint8_t addr;

	/* Local variables for values */
	float current, voltage, power, frequency, energy_in, energy_out;
	float voltage_l1, current_l1, power_l1;
	float voltage_l2, current_l2, power_l2;
	float voltage_l3, current_l3, power_l3;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling EM5XX at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Total values */
	if (read_int32_lsw(addr, REG_CURRENT, &current, 1000.0f)) errors++;
	if (read_int32_lsw(addr, REG_VOLTAGE, &voltage, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_POWER, &power, 10000.0f)) errors++;
	if (read_int32_lsw(addr, REG_FREQUENCY, &frequency, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_ENERGY_IN, &energy_in, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_ENERGY_OUT, &energy_out, 10.0f)) errors++;

	/* Phase L1 */
	if (read_int32_lsw(addr, REG_VOLTAGE_L1, &voltage_l1, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_CURRENT_L1, &current_l1, 1000.0f)) errors++;
	if (read_int32_lsw(addr, REG_POWER_L1, &power_l1, 10000.0f)) errors++;

	/* Phase L2 */
	if (read_int32_lsw(addr, REG_VOLTAGE_L2, &voltage_l2, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_CURRENT_L2, &current_l2, 1000.0f)) errors++;
	if (read_int32_lsw(addr, REG_POWER_L2, &power_l2, 10000.0f)) errors++;

	/* Phase L3 */
	if (read_int32_lsw(addr, REG_VOLTAGE_L3, &voltage_l3, 10.0f)) errors++;
	if (read_int32_lsw(addr, REG_CURRENT_L3, &current_l3, 1000.0f)) errors++;
	if (read_int32_lsw(addr, REG_POWER_L3, &power_l3, 10000.0f)) errors++;

	app_modbus_disable();

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.current = current;
		m_data.voltage = voltage;
		m_data.power = power;
		m_data.frequency = frequency;
		m_data.energy_in = energy_in;
		m_data.energy_out = energy_out;
		m_data.voltage_l1 = voltage_l1;
		m_data.current_l1 = current_l1;
		m_data.power_l1 = power_l1;
		m_data.voltage_l2 = voltage_l2;
		m_data.current_l2 = current_l2;
		m_data.power_l2 = power_l2;
		m_data.voltage_l3 = voltage_l3;
		m_data.current_l3 = current_l3;
		m_data.power_l3 = power_l3;
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
			m_samples[m_sample_count].voltage_l1 = voltage_l1;
			m_samples[m_sample_count].voltage_l2 = voltage_l2;
			m_samples[m_sample_count].voltage_l3 = voltage_l3;
			m_samples[m_sample_count].current_l1 = current_l1;
			m_samples[m_sample_count].current_l2 = current_l2;
			m_samples[m_sample_count].current_l3 = current_l3;
			m_samples[m_sample_count].power_l1 = power_l1;
			m_samples[m_sample_count].power_l2 = power_l2;
			m_samples[m_sample_count].power_l3 = power_l3;
			m_sample_count++;
			LOG_DBG("EM5XX: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("EM5XX: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("EM5XX: V=%.1f V, I=%.3f A, P=%.4f kW, E_in=%.1f kWh",
			(double)voltage, (double)current, (double)power, (double)energy_in);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("EM5XX: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	LOG_INF("EM5XX: Address change via Modbus not implemented");
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);
	LOG_INF("Sampling address set to %d", new_addr);

	if (parity_str) {
		char parity;
		int stop_bits;
		if (parse_parity(parity_str, &parity, &stop_bits) == 0) {
			LOG_INF("Parity: %c, stop bits: %d", parity, stop_bits);
		}
	}

	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device em5xx sample <addr>");
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
	if (argc < 2) {
		shell_error(shell, "Usage: device em5xx config <addr> [8E1]");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	config(addr, (argc >= 3) ? argv[2] : NULL);
	shell_print(shell, "Sampling address set to %d", addr);
	return 0;
}

static const struct shell_static_entry em5xx_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Set sampling address: config <addr> [8E1]", cmd_config, 2, 1),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry em5xx_shell_cmds = {
	.entry = em5xx_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_em5xx data_copy = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] EM5XX (EM540) @ addr %d:", idx, addr);
	shell_print(shell, "  Voltage:    %.1f V (L1=%.1f L2=%.1f L3=%.1f)",
		    (double)data_copy.voltage, (double)data_copy.voltage_l1,
		    (double)data_copy.voltage_l2, (double)data_copy.voltage_l3);
	shell_print(shell, "  Current:    %.3f A (L1=%.3f L2=%.3f L3=%.3f)",
		    (double)data_copy.current, (double)data_copy.current_l1,
		    (double)data_copy.current_l2, (double)data_copy.current_l3);
	shell_print(shell, "  Power:      %.4f kW (L1=%.4f L2=%.4f L3=%.4f)",
		    (double)data_copy.power, (double)data_copy.power_l1,
		    (double)data_copy.power_l2, (double)data_copy.power_l3);
	shell_print(shell, "  Frequency:  %.1f Hz", (double)data_copy.frequency);
	shell_print(shell, "  Energy IN:  %.1f kWh", (double)data_copy.energy_in);
	shell_print(shell, "  Energy OUT: %.1f kWh", (double)data_copy.energy_out);
}

const struct app_device_driver em5xx_driver = {
	.name = "em5xx",
	.type = APP_DEVICE_TYPE_EM5XX,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_em5xx *em5xx_get_data(void)
{
	return &m_data;
}

void em5xx_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t em5xx_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int em5xx_get_samples(struct em5xx_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void em5xx_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("EM5XX: Sample buffer cleared");
}

int em5xx_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
