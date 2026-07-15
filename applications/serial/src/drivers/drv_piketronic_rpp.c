/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_piketronic_rpp.h"
#include "drv_interface.h"
#include "../app_modbus.h"

#include <chester/ctr_rtc.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_piketronic_rpp, LOG_LEVEL_DBG);

#define MAX_SAMPLES 32
#define DEFAULT_ADDR 1

/* Register addresses (register number == Modbus address on this probe) */
#define REG_CONCENTRATION_TIME 1  /* UINT */
#define REG_CONCENTRATION      2  /* UINT32 (L at 2, H at 3) */
#define REG_TEMPERATURE        4  /* signed int8 in low byte */
#define REG_HUMIDITY           5  /* UINT */
#define REG_CONCENTRATION_DAY  17 /* UINT32 (L at 17, H at 18) */
#define REG_LIMIT              33 /* UINT (writable) */
#define REG_RECORD_INTERVAL    36 /* UINT (writable) */
#define REG_SPECTRUM_INTERVAL  37 /* UINT (writable) */
#define REG_ALGORITHM          38 /* UINT (writable) */
#define REG_IDENT_DEVICE       60 /* 10 ASCII */
#define REG_IDENT_VERSION      65 /* 10 ASCII */
#define REG_IDENT_SERIAL       70 /* 10 ASCII */

/* Calibration registers - writing these corrupts factory calibration.
 * Per the official register map (RPP-R_rs485_modbus_communication.pdf),
 * register 33 (REG_LIMIT) is documented as a normal writable alarm threshold,
 * not calibration - but this driver deliberately keeps the whole 28-35 range
 * blocked (including 33), so REG_LIMIT is not writable through this driver. */
#define REG_CALIB_FIRST        28
#define REG_CALIB_LAST         35

static struct app_data_piketronic_rpp m_data = {
	.modbus_addr = DEFAULT_ADDR,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

static struct piketronic_rpp_sample m_samples[MAX_SAMPLES];
static int m_sample_count = 0;
static K_MUTEX_DEFINE(m_samples_mutex);

/* Combine two 16-bit registers into a 32-bit value. The probe stores the
 * lower word (L) at the lower address and the higher word (H) at the next. */
static inline uint32_t decode_u32(const uint16_t *regs)
{
	return ((uint32_t)regs[1] << 16) | regs[0];
}

/* Decode a register-packed ASCII string. Two characters per register, low
 * byte first. out must hold count*2 + 1 bytes. The probe pads the unused part
 * of a field with literal '.' characters (e.g. the serial reads as
 * "24062....."), and may also leave non-printable bytes, so we drop any
 * non-printable bytes and strip trailing '.'/space/null padding. */
static void decode_ascii(const uint16_t *regs, int count, char *out)
{
	int j = 0;
	for (int i = 0; i < count; i++) {
		out[j++] = (char)(regs[i] & 0xFF);
		out[j++] = (char)(regs[i] >> 8);
	}
	out[j] = '\0';
	/* Terminate at the first non-printable byte (drops junk padding) */
	for (int k = 0; k < j; k++) {
		unsigned char c = (unsigned char)out[k];
		if (c < 0x20 || c > 0x7E) {
			out[k] = '\0';
			break;
		}
	}
	/* Strip trailing padding: '.', spaces and nulls (the probe fills the
	 * unused tail of the serial field with dots) */
	for (int k = (int)strlen(out) - 1;
	     k >= 0 && (out[k] == '.' || out[k] == ' ' || out[k] == '\0'); k--) {
		out[k] = '\0';
	}
}

/* Reject writes to the whole protected register block (see comment above
 * REG_CALIB_FIRST) - they must never be written by this driver. */
static int safe_write_reg(uint8_t addr, uint16_t reg, uint16_t val)
{
	if (reg >= REG_CALIB_FIRST && reg <= REG_CALIB_LAST) {
		LOG_ERR("Refusing to write calibration register %u", reg);
		return -EPERM;
	}
	return app_modbus_write_holding_reg(addr, reg, val);
}

/* Forward declaration */
static void print_data(const struct shell *shell, int idx, int addr);

static int init(void)
{
	LOG_INF("Initializing Piketronic RPP-R driver");
	m_data.valid = false;
	m_data.error_count = 0;
	return 0;
}

static int sample(void)
{
	int ret;
	int errors = 0;
	uint8_t addr;
	uint16_t regs[16];

	uint32_t concentration = 0, concentration_day = 0;
	int8_t temperature = 0;
	uint8_t humidity = 0;
	uint16_t concentration_time = 0;
	uint16_t limit = 0, record_interval = 0, spectrum_interval = 0, algorithm = 0;
	char device[11] = {0}, version_sw[11] = {0}, serial_number[11] = {0};

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);

	LOG_INF("Sampling Piketronic RPP-R at address %d", addr);

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		return ret;
	}

	/* Measurements: regs 1-5 (time, concentration L+H, temperature, humidity) */
	if (app_modbus_read_holding_regs(addr, REG_CONCENTRATION_TIME, 5, regs) == 0) {
		concentration_time = regs[0];
		concentration = decode_u32(&regs[1]); /* regs[1]=L (reg2), regs[2]=H (reg3) */
		temperature = (int8_t)(regs[3] & 0xFF);
		humidity = (uint8_t)(regs[4] & 0xFF);
	} else {
		errors++;
	}

	/* Daily average: regs 17-18 */
	if (app_modbus_read_holding_regs(addr, REG_CONCENTRATION_DAY, 2, regs) == 0) {
		concentration_day = decode_u32(regs);
	} else {
		errors++;
	}

	/* Alarm limit: reg 33 */
	if (app_modbus_read_holding_regs(addr, REG_LIMIT, 1, regs) == 0) {
		limit = regs[0];
	} else {
		errors++;
	}

	/* Intervals + algorithm: regs 36-38 */
	if (app_modbus_read_holding_regs(addr, REG_RECORD_INTERVAL, 3, regs) == 0) {
		record_interval = regs[0];
		spectrum_interval = regs[1];
		algorithm = regs[2];
	} else {
		errors++;
	}

	/* Identification: regs 60-74 (device, version, serial - 5 regs each) */
	if (app_modbus_read_holding_regs(addr, REG_IDENT_DEVICE, 15, regs) == 0) {
		decode_ascii(&regs[0], 5, device);
		decode_ascii(&regs[5], 5, version_sw);
		decode_ascii(&regs[10], 5, serial_number);
	} else {
		errors++;
	}

	app_modbus_disable();

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	if (errors == 0) {
		m_data.concentration = concentration;
		m_data.concentration_day = concentration_day;
		m_data.temperature = temperature;
		m_data.humidity = humidity;
		m_data.concentration_time = concentration_time;
		m_data.limit = limit;
		m_data.record_interval = record_interval;
		m_data.spectrum_interval = spectrum_interval;
		m_data.algorithm = algorithm;
		strcpy(m_data.device, device);
		strcpy(m_data.version_sw, version_sw);
		strcpy(m_data.serial_number, serial_number);
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
			m_samples[m_sample_count].concentration = concentration;
			m_samples[m_sample_count].concentration_day = concentration_day;
			m_samples[m_sample_count].temperature = temperature;
			m_samples[m_sample_count].humidity = humidity;
			m_sample_count++;
			LOG_DBG("RPP-R: Added sample %d to buffer", m_sample_count);
		} else {
			LOG_WRN("RPP-R: Sample buffer full");
		}
		k_mutex_unlock(&m_samples_mutex);

		LOG_INF("RPP-R: Rn=%u Bq/m3, T=%d C, RH=%u %%", concentration, temperature,
			humidity);
	} else {
		m_data.valid = false;
		m_data.error_count++;
		k_mutex_unlock(&m_data_mutex);
		LOG_WRN("RPP-R: %d read errors", errors);
		return -EIO;
	}

	return 0;
}

static int config(uint8_t new_addr, const char *parity_str)
{
	ARG_UNUSED(parity_str);
	/* Address and baud/parity are DIP switches on the probe; we only set
	 * which address CHESTER polls. */
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.modbus_addr = new_addr;
	k_mutex_unlock(&m_data_mutex);
	LOG_INF("RPP-R: sampling address set to %d", new_addr);
	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device piketronic sample <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t old_addr = m_data.modbus_addr;
	m_data.modbus_addr = addr;
	k_mutex_unlock(&m_data_mutex);

	int ret = sample();

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
		shell_error(shell, "Usage: device piketronic config <addr>");
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}

	config(addr, NULL);
	shell_print(shell, "Sampling address set to %d", addr);
	return 0;
}

/* Write one of the safe setting registers: limit / record / spectrum / mode */
static int write_setting(const struct shell *shell, size_t argc, char **argv, uint16_t reg,
			 const char *what)
{
	if (argc < 3) {
		shell_error(shell, "Usage: device piketronic %s <addr> <value>", what);
		return -EINVAL;
	}

	uint8_t addr = (uint8_t)strtol(argv[1], NULL, 0);
	if (addr < 1 || addr > 247) {
		shell_error(shell, "Invalid address (1-247)");
		return -EINVAL;
	}
	uint16_t val = (uint16_t)strtol(argv[2], NULL, 0);

	int ret = app_modbus_enable();
	if (ret) {
		shell_error(shell, "Failed to enable Modbus: %d", ret);
		return ret;
	}
	ret = safe_write_reg(addr, reg, val);
	app_modbus_disable();

	if (ret) {
		shell_error(shell, "Write failed: %d", ret);
		return ret;
	}
	shell_print(shell, "Set %s = %u on addr %d", what, val, addr);
	return 0;
}

static int cmd_limit(const struct shell *shell, size_t argc, char **argv)
{
	return write_setting(shell, argc, argv, REG_LIMIT, "limit");
}

static int cmd_interval(const struct shell *shell, size_t argc, char **argv)
{
	return write_setting(shell, argc, argv, REG_RECORD_INTERVAL, "interval");
}

static int cmd_mode(const struct shell *shell, size_t argc, char **argv)
{
	return write_setting(shell, argc, argv, REG_ALGORITHM, "mode");
}

static const struct shell_static_entry piketronic_rpp_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "Set sampling address: config <addr>", cmd_config, 2, 0),
	SHELL_CMD_ARG(limit, NULL, "Set alarm limit: limit <addr> <bq>", cmd_limit, 3, 0),
	SHELL_CMD_ARG(interval, NULL, "Set record interval (min): interval <addr> <min>",
		      cmd_interval, 3, 0),
	SHELL_CMD_ARG(mode, NULL, "Set calc mode: mode <addr> <0=RnA|1..255=RnA+RnC>", cmd_mode, 3,
		      0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry piketronic_rpp_shell_cmds = {
	.entry = piketronic_rpp_subcmds
};

static void print_data(const struct shell *shell, int idx, int addr)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	struct app_data_piketronic_rpp d = m_data;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "[%d] Piketronic RPP-R @ addr %d:", idx, addr);
	shell_print(shell, "  radon (1h avg):   %u Bq/m3", d.concentration);
	shell_print(shell, "  radon (1d avg):   %u Bq/m3", d.concentration_day);
	shell_print(shell, "  temperature:      %d C", d.temperature);
	shell_print(shell, "  humidity:         %u %%", d.humidity);
	shell_print(shell, "  interval timer:   %u s (into 4-min cycle)", d.concentration_time);
	shell_print(shell, "  alarm limit:      %u Bq/m3", d.limit);
	shell_print(shell, "  record interval:  %u min", d.record_interval);
	shell_print(shell, "  spectrum interval:%u min", d.spectrum_interval);
	shell_print(shell, "  calc mode:        %u (%s)", d.algorithm,
		    d.algorithm == 0 ? "RnA" : "RnA+RnC");
	shell_print(shell, "  device:           %s", d.device);
	shell_print(shell, "  version:          %s", d.version_sw);
	shell_print(shell, "  serial:           %s", d.serial_number);
}

const struct app_device_driver piketronic_rpp_driver = {
	.name = "piketronic",
	.type = APP_DEVICE_TYPE_PIKETRONIC_RPP,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = config,
	.print_data = print_data,
};

const struct app_data_piketronic_rpp *piketronic_rpp_get_data(void)
{
	return &m_data;
}

void piketronic_rpp_set_addr(uint8_t addr)
{
	if (addr >= 1 && addr <= 247) {
		k_mutex_lock(&m_data_mutex, K_FOREVER);
		m_data.modbus_addr = addr;
		k_mutex_unlock(&m_data_mutex);
	}
}

uint8_t piketronic_rpp_get_addr(void)
{
	k_mutex_lock(&m_data_mutex, K_FOREVER);
	uint8_t addr = m_data.modbus_addr;
	k_mutex_unlock(&m_data_mutex);
	return addr;
}

int piketronic_rpp_get_samples(struct piketronic_rpp_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void piketronic_rpp_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("RPP-R: Sample buffer cleared");
}

int piketronic_rpp_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}
