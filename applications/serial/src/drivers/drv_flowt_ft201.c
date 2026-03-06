/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_flowt_ft201.h"
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

LOG_MODULE_REGISTER(drv_flowt_ft201, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32

/* FlowT FT201 Register addresses (PDU / 0-based) */
#define REG_FLOW_S		0x0000 /* Float32 LSW, 2 regs */
#define REG_FLOW_M		0x0002
#define REG_FLOW_H		0x0004
#define REG_VELOCITY		0x0006
#define REG_POS_TOTAL_MANT	0x0008 /* Float32 LSW, 2 regs */
#define REG_POS_TOTAL_EXP	0x000A /* UINT16, 1 reg */
#define REG_NEG_TOTAL_MANT	0x000B
#define REG_NEG_TOTAL_EXP	0x000D
#define REG_NET_TOTAL_MANT	0x000E
#define REG_NET_TOTAL_EXP	0x0010
#define REG_ENERGY_HOT_MANT	0x0013
#define REG_ENERGY_HOT_EXP	0x0015
#define REG_ENERGY_COLD_MANT	0x0016
#define REG_ENERGY_COLD_EXP	0x0018
#define REG_SIGNAL_QUALITY	0x001D /* UINT16 */
#define REG_INSTRUMENT_ADDR	0x0043 /* 32-bit int, 2 regs */
#define REG_SERIAL_NUMBER	0x0045 /* String, 4 regs (8 chars) */
#define REG_TEMP_INLET		0x0049 /* Float32 LSW, 2 regs */
#define REG_TEMP_OUTLET		0x004B
#define REG_MODBUS_ADDR		0x1003 /* UINT16 R/W (FC06) */

#define DEFAULT_ADDR 1

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

static struct app_data_flowt_ft201 m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct flowt_ft201_sample m_samples[MAX_SAMPLES];
static int m_sample_count;
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

/* Read totalizer: mantissa (Float32 LSW, 2 regs) + exponent (UINT16, 1 reg) */
static int read_totalizer(uint8_t addr, uint16_t mant_reg, uint16_t exp_reg, float *out)
{
	uint16_t regs[2];
	uint16_t exp_val;
	int ret;

	ret = app_modbus_read_holding_regs(addr, mant_reg, 2, regs);
	if (ret) {
		return ret;
	}

	k_msleep(100);

	ret = app_modbus_read_holding_regs(addr, exp_reg, 1, &exp_val);
	if (ret) {
		return ret;
	}

	float mantissa = decode_float32_lsw(regs);
	int16_t exponent = (int16_t)exp_val;
	*out = mantissa * powf(10.0f, (float)exponent);
	return 0;
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing FlowT FT201 driver");
	m_data.valid = false;
	m_data.error_count = 0;
	return 0;
}

static int sample(void)
{
	int ret;
	int errors = 0;
	uint8_t addr;

	float flow_s, flow_m, flow_h, velocity;
	float total_positive, total_negative, total_net;
	float energy_hot, energy_cold;
	float temp_inlet, temp_outlet;
	uint16_t signal_quality;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling FlowT FT201 at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Flow registers (100ms delay between consecutive reads for SC16IS740) */
	if (read_float32_lsw(addr, REG_FLOW_S, &flow_s)) {
		errors++;
	}
	k_msleep(100);
	if (read_float32_lsw(addr, REG_FLOW_M, &flow_m)) {
		errors++;
	}
	k_msleep(100);
	if (read_float32_lsw(addr, REG_FLOW_H, &flow_h)) {
		errors++;
	}
	k_msleep(100);
	if (read_float32_lsw(addr, REG_VELOCITY, &velocity)) {
		errors++;
	}
	k_msleep(100);

	/* Totalizer registers */
	if (read_totalizer(addr, REG_POS_TOTAL_MANT, REG_POS_TOTAL_EXP, &total_positive)) {
		errors++;
	}
	k_msleep(100);
	if (read_totalizer(addr, REG_NEG_TOTAL_MANT, REG_NEG_TOTAL_EXP, &total_negative)) {
		errors++;
	}
	k_msleep(100);
	if (read_totalizer(addr, REG_NET_TOTAL_MANT, REG_NET_TOTAL_EXP, &total_net)) {
		errors++;
	}
	k_msleep(100);

	/* Energy registers */
	if (read_totalizer(addr, REG_ENERGY_HOT_MANT, REG_ENERGY_HOT_EXP, &energy_hot)) {
		errors++;
	}
	k_msleep(100);
	if (read_totalizer(addr, REG_ENERGY_COLD_MANT, REG_ENERGY_COLD_EXP, &energy_cold)) {
		errors++;
	}
	k_msleep(100);

	/* Temperature registers */
	if (read_float32_lsw(addr, REG_TEMP_INLET, &temp_inlet)) {
		errors++;
	}
	k_msleep(100);
	if (read_float32_lsw(addr, REG_TEMP_OUTLET, &temp_outlet)) {
		errors++;
	}
	k_msleep(100);

	/* Signal quality */
	ret = app_modbus_read_holding_regs(addr, REG_SIGNAL_QUALITY, 1, &signal_quality);
	if (ret) {
		errors++;
	}

	app_modbus_disable();

	/* Update m_data atomically */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.flow_s = flow_s;
		m_data.flow_m = flow_m;
		m_data.flow_h = flow_h;
		m_data.velocity = velocity;
		m_data.total_positive = total_positive;
		m_data.total_negative = total_negative;
		m_data.total_net = total_net;
		m_data.energy_hot = energy_hot;
		m_data.energy_cold = energy_cold;
		m_data.temp_inlet = temp_inlet;
		m_data.temp_outlet = temp_outlet;
		m_data.signal_quality = (uint8_t)(signal_quality & 0xFF);
		m_data.valid = true;
		m_data.last_sample = k_uptime_get_32();
		m_data.error_count = 0;
		k_mutex_unlock(&m_data_mutex);

		/* Add to sample buffer */
		k_mutex_lock(&m_samples_mutex, K_FOREVER);
		if (m_sample_count < MAX_SAMPLES) {
			uint64_t ts;
			ctr_rtc_get_ts(&ts);

			struct flowt_ft201_sample *s = &m_samples[m_sample_count];
			s->timestamp = ts;
			s->flow_s = flow_s;
			s->flow_m = flow_m;
			s->flow_h = flow_h;
			s->velocity = velocity;
			s->total_positive = total_positive;
			s->total_negative = total_negative;
			s->total_net = total_net;
			s->energy_hot = energy_hot;
			s->energy_cold = energy_cold;
			s->temp_inlet = temp_inlet;
			s->temp_outlet = temp_outlet;
			s->signal_quality = (uint8_t)(signal_quality & 0xFF);
			m_sample_count++;
			LOG_DBG("FlowT FT201: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("FlowT FT201: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("FlowT FT201: flow_h=%.4f m3/h, vel=%.4f m/s, "
			"T_in=%.1f C, T_out=%.1f C, SQ=%u",
			(double)flow_h, (double)velocity,
			(double)temp_inlet, (double)temp_outlet,
			signal_quality & 0xFF);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("FlowT FT201: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	int ret;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t old_addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("FlowT FT201: Writing new address %d (current: %d)", new_addr, old_addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		return ret;
	}

	/* Write new address via FC06 to register 0x1003 */
	ret = app_modbus_write_holding_reg(old_addr, REG_MODBUS_ADDR, (uint16_t)new_addr);

	app_modbus_disable();

	if (ret) {
		LOG_ERR("Failed to write address: %d", ret);
		return ret;
	}

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("FlowT FT201: Address changed to %d", new_addr);
	return 0;
}

static int cmd_read_info(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device flowt_ft201 read_info <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	int ret = app_modbus_enable();
	if (ret) {
		shell_error(shell, "Failed to enable Modbus: %d", ret);
		return ret;
	}

	/* Read serial number (4 regs = 8 chars, split into 2x2 reads) */
	uint16_t sn_regs[4];
	ret = app_modbus_read_holding_regs(addr, REG_SERIAL_NUMBER, 2, &sn_regs[0]);
	if (ret) {
		shell_error(shell, "Failed to read serial number (part 1): %d", ret);
		app_modbus_disable();
		return ret;
	}
	k_msleep(100);

	ret = app_modbus_read_holding_regs(addr, REG_SERIAL_NUMBER + 2, 2, &sn_regs[2]);
	if (ret) {
		shell_error(shell, "Failed to read serial number (part 2): %d", ret);
		app_modbus_disable();
		return ret;
	}

	k_msleep(100);

	char serial[9];
	serial[0] = (sn_regs[0] >> 8) & 0xFF;
	serial[1] = sn_regs[0] & 0xFF;
	serial[2] = (sn_regs[1] >> 8) & 0xFF;
	serial[3] = sn_regs[1] & 0xFF;
	serial[4] = (sn_regs[2] >> 8) & 0xFF;
	serial[5] = sn_regs[2] & 0xFF;
	serial[6] = (sn_regs[3] >> 8) & 0xFF;
	serial[7] = sn_regs[3] & 0xFF;
	serial[8] = '\0';

	/* Read instrument address (2 regs, 32-bit) */
	uint16_t inst_regs[2];
	ret = app_modbus_read_holding_regs(addr, REG_INSTRUMENT_ADDR, 2, inst_regs);
	if (ret) {
		shell_error(shell, "Failed to read instrument address: %d", ret);
		app_modbus_disable();
		return ret;
	}
	uint32_t inst_addr = ((uint32_t)inst_regs[1] << 16) | inst_regs[0];

	app_modbus_disable();

	shell_print(shell, "FlowT FT201 @ addr %d:", addr);
	shell_print(shell, "  Serial number:     %s", serial);
	shell_print(shell, "  Instrument addr:   %u (0x%08X)", inst_addr, inst_addr);

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(shell, "Usage: device flowt_ft201 config <current_addr> <new_addr>");
		return -EINVAL;
	}

	uint8_t current_addr = (uint8_t)strtol(argv[1], NULL, 0);
	uint8_t new_addr = (uint8_t)strtol(argv[2], NULL, 0);

	if (current_addr < 1 || current_addr > 247 || new_addr < 1 || new_addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	/* Temporarily set address to current device */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t saved_addr = m_data.modbus_addr;
	m_data.modbus_addr = current_addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = config(new_addr, NULL);

	/* Restore saved address if config failed */
	if (ret) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = saved_addr;
		k_mutex_unlock(&m_data_mutex);
		shell_error(shell, "Config failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Address changed from %d to %d", current_addr, new_addr);
	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device flowt_ft201 sample <addr>");
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

static const struct shell_static_entry flowt_ft201_subcmds[] = {
	SHELL_CMD_ARG(read_info, NULL, "Read device info: read_info <addr>", cmd_read_info, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Change address: config <current_addr> <new_addr>", cmd_config,
		      3, 0),
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry flowt_ft201_shell_cmds = {
	.entry = flowt_ft201_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_flowt_ft201 d = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] FlowT FT201 @ addr %d:", idx, addr);
	shell_print(shell, "  Flow/s:          %.6f m3/s", (double)d.flow_s);
	shell_print(shell, "  Flow/m:          %.4f m3/min", (double)d.flow_m);
	shell_print(shell, "  Flow/h:          %.4f m3/h", (double)d.flow_h);
	shell_print(shell, "  Velocity:        %.4f m/s", (double)d.velocity);
	shell_print(shell, "  Positive total:  %.3f", (double)d.total_positive);
	shell_print(shell, "  Negative total:  %.3f", (double)d.total_negative);
	shell_print(shell, "  Net total:       %.3f", (double)d.total_net);
	shell_print(shell, "  Energy hot:      %.3f", (double)d.energy_hot);
	shell_print(shell, "  Energy cold:     %.3f", (double)d.energy_cold);
	shell_print(shell, "  Temp inlet:      %.1f C", (double)d.temp_inlet);
	shell_print(shell, "  Temp outlet:     %.1f C", (double)d.temp_outlet);
	shell_print(shell, "  Signal quality:  %u", d.signal_quality);
}

const struct app_device_driver flowt_ft201_driver = {
	.name = "flowt_ft201",
	.type = APP_DEVICE_TYPE_FLOWT_FT201,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_flowt_ft201 *flowt_ft201_get_data(void)
{
	return &m_data;
}

void flowt_ft201_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t flowt_ft201_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int flowt_ft201_get_samples(struct flowt_ft201_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void flowt_ft201_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("FlowT FT201: Sample buffer cleared");
}

int flowt_ft201_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
