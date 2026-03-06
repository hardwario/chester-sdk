/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_iem3000.h"
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

LOG_MODULE_REGISTER(drv_iem3000, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

/* Decode Float32 Low Word First (word-swap) */
static float decode_float32_lsw(const uint16_t *regs)
{
	union {
		uint32_t u;
		float f;
	} conv;
	conv.u = ((uint32_t)regs[1] << 16) | regs[0];
	return conv.f;
}

/* Decode INT64 Big Endian */
static int64_t decode_int64_be(const uint16_t *regs)
{
	return ((int64_t)regs[0] << 48) |
	       ((int64_t)regs[1] << 32) |
	       ((int64_t)regs[2] << 16) |
	       ((int64_t)regs[3]);
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

/* iEM3000 Register addresses (Float32 word-swap, INT64 for energy) */
#define REG_CURRENT_L1        0x0BB7
#define REG_CURRENT_L2        0x0BB9
#define REG_CURRENT_L3        0x0BBB
#define REG_CURRENT           0x0BC1
#define REG_VOLTAGE_L1        0x0BD3
#define REG_VOLTAGE_L2        0x0BD5
#define REG_VOLTAGE_L3        0x0BD7
#define REG_POWER_L1          0x0BED
#define REG_POWER_L2          0x0BEF
#define REG_POWER_L3          0x0BF1
#define REG_POWER             0x0BF3
#define REG_POWER_REACTIVE    0x0BFB
#define REG_POWER_APPARENT    0x0C03
#define REG_POWER_FACTOR      0x0C0B
#define REG_FREQUENCY         0x0C25
#define REG_ENERGY_IN         0x0C83  /* INT64, /1000 kWh */
#define REG_ENERGY_OUT        0x0C87  /* INT64, /1000 kWh */
#define REG_ENERGY_REACTIVE_IN  0x0C93
#define REG_ENERGY_REACTIVE_OUT 0x0C97

#define DEFAULT_ADDR          1

static struct app_data_iem3000 m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct iem3000_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

static int read_float32_lsw(uint8_t addr, uint16_t reg, float *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_holding_regs(addr, reg, 2, regs);
	if (ret) {
		return ret;
	}
	*out = decode_float32_lsw(regs);
	return 0;
}

static int read_int64_energy(uint8_t addr, uint16_t reg, float *out)
{
	uint16_t regs[4];
	int ret;

	/* Read 4 registers for INT64 */
	ret = app_modbus_read_holding_regs(addr, reg, 2, &regs[0]);
	if (ret) {
		return ret;
	}
	ret = app_modbus_read_holding_regs(addr, reg + 2, 2, &regs[2]);
	if (ret) {
		return ret;
	}

	int64_t val = decode_int64_be(regs);
	*out = (float)val / 1000.0f;
	return 0;
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing iEM3000 driver");
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
	float current, current_l1, current_l2, current_l3;
	float voltage_l1, voltage_l2, voltage_l3;
	float power, power_l1, power_l2, power_l3;
	float power_reactive, power_apparent, power_factor, frequency;
	float energy_in, energy_out, energy_reactive_in, energy_reactive_out;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling iEM3000 at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Per-phase currents */
	if (read_float32_lsw(addr, REG_CURRENT_L1, &current_l1)) errors++;
	if (read_float32_lsw(addr, REG_CURRENT_L2, &current_l2)) errors++;
	if (read_float32_lsw(addr, REG_CURRENT_L3, &current_l3)) errors++;
	if (read_float32_lsw(addr, REG_CURRENT, &current)) errors++;

	/* Per-phase voltages */
	if (read_float32_lsw(addr, REG_VOLTAGE_L1, &voltage_l1)) errors++;
	if (read_float32_lsw(addr, REG_VOLTAGE_L2, &voltage_l2)) errors++;
	if (read_float32_lsw(addr, REG_VOLTAGE_L3, &voltage_l3)) errors++;

	/* Power */
	if (read_float32_lsw(addr, REG_POWER_L1, &power_l1)) errors++;
	if (read_float32_lsw(addr, REG_POWER_L2, &power_l2)) errors++;
	if (read_float32_lsw(addr, REG_POWER_L3, &power_l3)) errors++;
	if (read_float32_lsw(addr, REG_POWER, &power)) errors++;
	if (read_float32_lsw(addr, REG_POWER_REACTIVE, &power_reactive)) errors++;
	if (read_float32_lsw(addr, REG_POWER_APPARENT, &power_apparent)) errors++;
	if (read_float32_lsw(addr, REG_POWER_FACTOR, &power_factor)) errors++;
	if (read_float32_lsw(addr, REG_FREQUENCY, &frequency)) errors++;

	/* Energy (INT64) */
	if (read_int64_energy(addr, REG_ENERGY_IN, &energy_in)) errors++;
	if (read_int64_energy(addr, REG_ENERGY_OUT, &energy_out)) errors++;

	/* Reactive energy (Float32) */
	if (read_float32_lsw(addr, REG_ENERGY_REACTIVE_IN, &energy_reactive_in)) errors++;
	if (read_float32_lsw(addr, REG_ENERGY_REACTIVE_OUT, &energy_reactive_out)) errors++;

	app_modbus_disable();

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.current = current;
		m_data.current_l1 = current_l1;
		m_data.current_l2 = current_l2;
		m_data.current_l3 = current_l3;
		m_data.voltage_l1 = voltage_l1;
		m_data.voltage_l2 = voltage_l2;
		m_data.voltage_l3 = voltage_l3;
		m_data.power = power;
		m_data.power_l1 = power_l1;
		m_data.power_l2 = power_l2;
		m_data.power_l3 = power_l3;
		m_data.power_reactive = power_reactive;
		m_data.power_apparent = power_apparent;
		m_data.power_factor = power_factor;
		m_data.frequency = frequency;
		m_data.energy_in = energy_in;
		m_data.energy_out = energy_out;
		m_data.energy_reactive_in = energy_reactive_in;
		m_data.energy_reactive_out = energy_reactive_out;
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
			m_samples[m_sample_count].voltage_l1 = voltage_l1;
			m_samples[m_sample_count].voltage_l2 = voltage_l2;
			m_samples[m_sample_count].voltage_l3 = voltage_l3;
			m_samples[m_sample_count].current_l1 = current_l1;
			m_samples[m_sample_count].current_l2 = current_l2;
			m_samples[m_sample_count].current_l3 = current_l3;
			m_samples[m_sample_count].power = power;
			m_samples[m_sample_count].energy_in = energy_in;
			m_samples[m_sample_count].energy_out = energy_out;
			m_sample_count++;
			LOG_DBG("iEM3000: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("iEM3000: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("iEM3000: P=%.1f W, E_in=%.1f kWh, PF=%.2f",
			(double)power, (double)energy_in, (double)power_factor);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("iEM3000: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	LOG_INF("iEM3000: Address change via Modbus not implemented");
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
		shell_error(shell, "Usage: device iem3000 sample <addr>");
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
		shell_error(shell, "Usage: device iem3000 config <addr> [8E1]");
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

static const struct shell_static_entry iem3000_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Set sampling address: config <addr> [8E1]", cmd_config, 2, 1),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry iem3000_shell_cmds = {
	.entry = iem3000_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_iem3000 data_copy = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] iEM3000 @ addr %d:", idx, addr);
	shell_print(shell, "  Voltage:    L1=%.1f L2=%.1f L3=%.1f V",
		    (double)data_copy.voltage_l1, (double)data_copy.voltage_l2,
		    (double)data_copy.voltage_l3);
	shell_print(shell, "  Current:    %.2f A (L1=%.2f L2=%.2f L3=%.2f)",
		    (double)data_copy.current, (double)data_copy.current_l1,
		    (double)data_copy.current_l2, (double)data_copy.current_l3);
	shell_print(shell, "  Power:      %.1f W (L1=%.1f L2=%.1f L3=%.1f)",
		    (double)data_copy.power, (double)data_copy.power_l1,
		    (double)data_copy.power_l2, (double)data_copy.power_l3);
	shell_print(shell, "  Reactive:   %.1f kVAR", (double)data_copy.power_reactive);
	shell_print(shell, "  Apparent:   %.1f kVA", (double)data_copy.power_apparent);
	shell_print(shell, "  PF:         %.3f", (double)data_copy.power_factor);
	shell_print(shell, "  Frequency:  %.1f Hz", (double)data_copy.frequency);
	shell_print(shell, "  Energy IN:  %.1f kWh", (double)data_copy.energy_in);
	shell_print(shell, "  Energy OUT: %.1f kWh", (double)data_copy.energy_out);
	shell_print(shell, "  Reactive E: IN=%.1f OUT=%.1f kvarh",
		    (double)data_copy.energy_reactive_in, (double)data_copy.energy_reactive_out);
}

const struct app_device_driver iem3000_driver = {
	.name = "iem3000",
	.type = APP_DEVICE_TYPE_IEM3000,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_iem3000 *iem3000_get_data(void)
{
	return &m_data;
}

void iem3000_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t iem3000_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int iem3000_get_samples(struct iem3000_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void iem3000_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("iEM3000: Sample buffer cleared");
}

int iem3000_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
