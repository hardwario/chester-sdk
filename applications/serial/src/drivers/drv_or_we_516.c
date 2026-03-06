/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_or_we_516.h"
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

LOG_MODULE_REGISTER(drv_or_we_516, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

/* Decode Float32 Big Endian (standard Modbus format) */
static float decode_float32_be(const uint16_t *regs)
{
	union {
		uint32_t u;
		float f;
	} conv;
	conv.u = ((uint32_t)regs[0] << 16) | regs[1];
	return conv.f;
}

/* OR-WE-516 Register addresses (Float32 BE) */
#define REG_VOLTAGE_L1        0x000E
#define REG_VOLTAGE_L2        0x0010
#define REG_VOLTAGE_L3        0x0012
#define REG_FREQUENCY         0x0014
#define REG_CURRENT_L1        0x0016
#define REG_CURRENT_L2        0x0018
#define REG_CURRENT_L3        0x001A
#define REG_POWER             0x001C
#define REG_POWER_L1          0x001E
#define REG_POWER_L2          0x0020
#define REG_POWER_L3          0x0022
#define REG_POWER_REACTIVE    0x0024
#define REG_POWER_APPARENT    0x002C
#define REG_POWER_FACTOR      0x0034
#define REG_ENERGY            0x0100
#define REG_ENERGY_L1         0x0102
#define REG_ENERGY_L2         0x0104
#define REG_ENERGY_L3         0x0106
#define REG_ENERGY_IN         0x0108
#define REG_ENERGY_OUT        0x0110
#define REG_ENERGY_REACTIVE   0x0118
#define REG_ENERGY_REACTIVE_IN  0x0120
#define REG_ENERGY_REACTIVE_OUT 0x0128

#define DEFAULT_ADDR          1

static struct app_data_or_we_516 m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct or_we_516_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

static int read_float32(uint8_t addr, uint16_t reg, float *out)
{
	uint16_t regs[2];
	int ret = app_modbus_read_holding_regs(addr, reg, 2, regs);
	if (ret) {
		return ret;
	}
	*out = decode_float32_be(regs);
	return 0;
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing OR-WE-516 driver");
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
	float voltage_l1, voltage_l2, voltage_l3, frequency;
	float current_l1, current_l2, current_l3;
	float power, power_l1, power_l2, power_l3;
	float power_reactive, power_apparent, power_factor;
	float energy, energy_l1, energy_l2, energy_l3, energy_in, energy_out;
	float energy_reactive, energy_reactive_in, energy_reactive_out;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling OR-WE-516 at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Per-phase voltages */
	if (read_float32(addr, REG_VOLTAGE_L1, &voltage_l1)) errors++;
	if (read_float32(addr, REG_VOLTAGE_L2, &voltage_l2)) errors++;
	if (read_float32(addr, REG_VOLTAGE_L3, &voltage_l3)) errors++;

	/* Frequency */
	if (read_float32(addr, REG_FREQUENCY, &frequency)) errors++;

	/* Per-phase currents */
	if (read_float32(addr, REG_CURRENT_L1, &current_l1)) errors++;
	if (read_float32(addr, REG_CURRENT_L2, &current_l2)) errors++;
	if (read_float32(addr, REG_CURRENT_L3, &current_l3)) errors++;

	/* Power */
	if (read_float32(addr, REG_POWER, &power)) errors++;
	if (read_float32(addr, REG_POWER_L1, &power_l1)) errors++;
	if (read_float32(addr, REG_POWER_L2, &power_l2)) errors++;
	if (read_float32(addr, REG_POWER_L3, &power_l3)) errors++;
	if (read_float32(addr, REG_POWER_REACTIVE, &power_reactive)) errors++;
	if (read_float32(addr, REG_POWER_APPARENT, &power_apparent)) errors++;
	if (read_float32(addr, REG_POWER_FACTOR, &power_factor)) errors++;

	/* Energy */
	if (read_float32(addr, REG_ENERGY, &energy)) errors++;
	if (read_float32(addr, REG_ENERGY_L1, &energy_l1)) errors++;
	if (read_float32(addr, REG_ENERGY_L2, &energy_l2)) errors++;
	if (read_float32(addr, REG_ENERGY_L3, &energy_l3)) errors++;
	if (read_float32(addr, REG_ENERGY_IN, &energy_in)) errors++;
	if (read_float32(addr, REG_ENERGY_OUT, &energy_out)) errors++;
	if (read_float32(addr, REG_ENERGY_REACTIVE, &energy_reactive)) errors++;
	if (read_float32(addr, REG_ENERGY_REACTIVE_IN, &energy_reactive_in)) errors++;
	if (read_float32(addr, REG_ENERGY_REACTIVE_OUT, &energy_reactive_out)) errors++;

	app_modbus_disable();

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.voltage_l1 = voltage_l1;
		m_data.voltage_l2 = voltage_l2;
		m_data.voltage_l3 = voltage_l3;
		m_data.frequency = frequency;
		m_data.current_l1 = current_l1;
		m_data.current_l2 = current_l2;
		m_data.current_l3 = current_l3;
		m_data.power = power;
		m_data.power_l1 = power_l1;
		m_data.power_l2 = power_l2;
		m_data.power_l3 = power_l3;
		m_data.power_reactive = power_reactive;
		m_data.power_apparent = power_apparent;
		m_data.power_factor = power_factor;
		m_data.energy = energy;
		m_data.energy_l1 = energy_l1;
		m_data.energy_l2 = energy_l2;
		m_data.energy_l3 = energy_l3;
		m_data.energy_in = energy_in;
		m_data.energy_out = energy_out;
		m_data.energy_reactive = energy_reactive;
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
			m_samples[m_sample_count].power_l1 = power_l1;
			m_samples[m_sample_count].power_l2 = power_l2;
			m_samples[m_sample_count].power_l3 = power_l3;
			m_samples[m_sample_count].power = power;
			m_samples[m_sample_count].energy = energy;
			m_sample_count++;
			LOG_DBG("OR-WE-516: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("OR-WE-516: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("OR-WE-516: P=%.2f kW, E=%.1f kWh, PF=%.2f",
			(double)power, (double)energy, (double)power_factor);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("OR-WE-516: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	LOG_INF("OR-WE-516: Parity is hardware fixed (8E1)");
	LOG_INF("Address change requires DIP switch configuration");

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);
	LOG_INF("Sampling address set to %d", new_addr);

	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device or_we_516 sample <addr>");
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
		shell_error(shell, "Usage: device or_we_516 config <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	config(addr, NULL);
	shell_print(shell, "Sampling address set to %d (parity fixed to 8E1)", addr);
	return 0;
}

static const struct shell_static_entry or_we_516_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Set sampling address: config <addr>", cmd_config, 2, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry or_we_516_shell_cmds = {
	.entry = or_we_516_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_or_we_516 data_copy = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] OR-WE-516 @ addr %d:", idx, addr);
	shell_print(shell, "  Voltage:    L1=%.1f L2=%.1f L3=%.1f V",
		    (double)data_copy.voltage_l1, (double)data_copy.voltage_l2,
		    (double)data_copy.voltage_l3);
	shell_print(shell, "  Current:    L1=%.2f L2=%.2f L3=%.2f A",
		    (double)data_copy.current_l1, (double)data_copy.current_l2,
		    (double)data_copy.current_l3);
	shell_print(shell, "  Power:      %.2f kW (L1=%.2f L2=%.2f L3=%.2f)",
		    (double)data_copy.power, (double)data_copy.power_l1,
		    (double)data_copy.power_l2, (double)data_copy.power_l3);
	shell_print(shell, "  Reactive:   %.2f kvar", (double)data_copy.power_reactive);
	shell_print(shell, "  Apparent:   %.2f kVA", (double)data_copy.power_apparent);
	shell_print(shell, "  PF:         %.3f", (double)data_copy.power_factor);
	shell_print(shell, "  Frequency:  %.1f Hz", (double)data_copy.frequency);
	shell_print(shell, "  Energy:     %.1f kWh (IN=%.1f OUT=%.1f)",
		    (double)data_copy.energy, (double)data_copy.energy_in,
		    (double)data_copy.energy_out);
	shell_print(shell, "  Reactive E: %.1f kvarh (IN=%.1f OUT=%.1f)",
		    (double)data_copy.energy_reactive, (double)data_copy.energy_reactive_in,
		    (double)data_copy.energy_reactive_out);
}

const struct app_device_driver or_we_516_driver = {
	.name = "or_we_516",
	.type = APP_DEVICE_TYPE_OR_WE_516,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_or_we_516 *or_we_516_get_data(void)
{
	return &m_data;
}

void or_we_516_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t or_we_516_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int or_we_516_get_samples(struct or_we_516_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void or_we_516_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("OR-WE-516: Sample buffer cleared");
}

int or_we_516_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
