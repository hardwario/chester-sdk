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

static int read_float32_lsw(uint8_t addr, uint16_t reg, float *out)
{
	return app_modbus_read_float(addr, reg, APP_MODBUS_FC_HOLDING, APP_MODBUS_WO_LO_FIRST, out);
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

	float mantissa = app_modbus_decode_float(regs, APP_MODBUS_WO_LO_FIRST);
	int16_t exponent = (int16_t)exp_val;
	*out = mantissa * powf(10.0f, (float)exponent);
	return 0;
}

static int init(struct app_driver_slot *slot)
{
	struct app_data_flowt_ft201 *data = slot->ctx;

	LOG_INF("Initializing FlowT FT201 driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_flowt_ft201 *data = slot->ctx;
	uint8_t addr = slot->addr;
	int ret;
	int errors = 0;

	float flow_s, flow_m, flow_h, velocity;
	float total_positive, total_negative, total_net;
	float energy_hot, energy_cold;
	float temp_inlet, temp_outlet;
	uint16_t signal_quality;

	LOG_INF("Sampling FlowT FT201 at address %d", addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
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

	app_modbus_release();

	if (errors) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("FlowT FT201: %d read errors", errors);
		return -EIO;
	}

	data->flow_s = flow_s;
	data->flow_m = flow_m;
	data->flow_h = flow_h;
	data->velocity = velocity;
	data->total_positive = total_positive;
	data->total_negative = total_negative;
	data->total_net = total_net;
	data->energy_hot = energy_hot;
	data->energy_cold = energy_cold;
	data->temp_inlet = temp_inlet;
	data->temp_outlet = temp_outlet;
	data->signal_quality = (uint8_t)(signal_quality & 0xFF);
	data->valid = true;
	data->last_sample = k_uptime_get_32();
	data->error_count = 0;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct flowt_ft201_sample rec = {
		.timestamp = ts,
		.flow_s = flow_s,
		.flow_m = flow_m,
		.flow_h = flow_h,
		.velocity = velocity,
		.total_positive = total_positive,
		.total_negative = total_negative,
		.total_net = total_net,
		.energy_hot = energy_hot,
		.energy_cold = energy_cold,
		.temp_inlet = temp_inlet,
		.temp_outlet = temp_outlet,
		.signal_quality = (uint8_t)(signal_quality & 0xFF),
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("FlowT FT201: flow_h=%.4f m3/h, vel=%.4f m/s, "
		"T_in=%.1f C, T_out=%.1f C, SQ=%u",
		(double)flow_h, (double)velocity, (double)temp_inlet, (double)temp_outlet,
		signal_quality & 0xFF);

	return 0;
}

static int config(struct app_driver_slot *slot, uint8_t new_addr, const char *parity_str)
{
	int ret;
	uint8_t old_addr = slot->addr;

	ARG_UNUSED(parity_str);

	LOG_INF("FlowT FT201: Writing new address %d (current: %d)", new_addr, old_addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		return ret;
	}

	/* Write new address via FC06 to register 0x1003 */
	ret = app_modbus_write_holding_reg(old_addr, REG_MODBUS_ADDR, (uint16_t)new_addr);

	app_modbus_release();

	if (ret) {
		LOG_ERR("Failed to write address: %d", ret);
		return ret;
	}

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

	int ret = app_modbus_acquire();
	if (ret) {
		shell_error(shell, "Failed to enable Modbus: %d", ret);
		return ret;
	}

	/* Read serial number (4 regs = 8 chars, split into 2x2 reads) */
	uint16_t sn_regs[4];
	ret = app_modbus_read_holding_regs(addr, REG_SERIAL_NUMBER, 2, &sn_regs[0]);
	if (ret) {
		shell_error(shell, "Failed to read serial number (part 1): %d", ret);
		app_modbus_release();
		return ret;
	}
	k_msleep(100);

	ret = app_modbus_read_holding_regs(addr, REG_SERIAL_NUMBER + 2, 2, &sn_regs[2]);
	if (ret) {
		shell_error(shell, "Failed to read serial number (part 2): %d", ret);
		app_modbus_release();
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
		app_modbus_release();
		return ret;
	}
	uint32_t inst_addr = app_modbus_decode_u32(inst_regs, APP_MODBUS_WO_LO_FIRST);

	app_modbus_release();

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

	int ret = app_device_probe_config(shell, &flowt_ft201_driver, current_addr, new_addr, NULL);
	if (ret) {
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

	int ret = app_device_probe_sample(shell, &flowt_ft201_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

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

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_flowt_ft201 *d = slot->ctx;

	app_device_print_header(shell, slot, "FlowT FT201");
	shell_print(shell, "  Flow/s:          %.6f m3/s", (double)d->flow_s);
	shell_print(shell, "  Flow/m:          %.4f m3/min", (double)d->flow_m);
	shell_print(shell, "  Flow/h:          %.4f m3/h", (double)d->flow_h);
	shell_print(shell, "  Velocity:        %.4f m/s", (double)d->velocity);
	shell_print(shell, "  Positive total:  %.3f", (double)d->total_positive);
	shell_print(shell, "  Negative total:  %.3f", (double)d->total_negative);
	shell_print(shell, "  Net total:       %.3f", (double)d->total_net);
	shell_print(shell, "  Energy hot:      %.3f", (double)d->energy_hot);
	shell_print(shell, "  Energy cold:     %.3f", (double)d->energy_cold);
	shell_print(shell, "  Temp inlet:      %.1f C", (double)d->temp_inlet);
	shell_print(shell, "  Temp outlet:     %.1f C", (double)d->temp_outlet);
	shell_print(shell, "  Signal quality:  %u", d->signal_quality);
}

const struct app_device_driver flowt_ft201_driver = {
	.name = "flowt_ft201",
	.type = APP_DEVICE_TYPE_FLOWT_FT201,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_flowt_ft201),
	.rec_size = sizeof(struct flowt_ft201_sample),
	.init = init,
	.sample = sample,
	.config = config,
	.print_data = print_data,
};

const struct app_data_flowt_ft201 *flowt_ft201_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
