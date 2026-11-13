/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * POWER BUDGET - RPP-R deployments are NOT battery powered. Do not size a battery for
 * one, and do not spend time trying to optimise the firmware down to it.
 *
 * Measured on CHESTER-M (PPK2, 3.6 V source meter) with three RPP-R probes on the RS-485
 * bus, firmware otherwise in its low-power configuration:
 *
 *   settled average          5.55 mA
 *   lowest 1 s bucket        5.43 mA   <- never drops below this
 *   same board, no probes    ~0.19 mA
 *   LS 26500 (7700 mAh)      ~0.16 years (about two months)
 *
 * The floor never falls, so this is a continuous load of roughly 5.3 mA on top of the
 * firmware baseline - about 1.7 mA per probe, consistent with an RS-485 transceiver held
 * in receive mode plus bus bias/termination. It is drawn by the probes, not by the
 * application: suspending the SC16IS7xx bridge does not change it.
 *
 * It cannot be reduced in firmware, for two independent reasons:
 *
 *   1. The probes are powered from the CHESTER, and the X2 Modbus shield exposes no
 *      power-enable line - the overlay defines only the SC16IS740 UART and the Modbus
 *      DE/RE GPIOs. There is nothing to switch off.
 *
 *   2. Even with a switchable rail it would be wrong. Radon measurement is integrative:
 *      the probe accumulates counts continuously and derives its moving averages and
 *      internal record log from that history. Cutting power resets the probe and throws
 *      that away, so the probes must stay powered between reads regardless of cost.
 *
 * Consequence: an RPP-R installation needs an external supply. Lowering interval-sample
 * changes almost nothing - a read takes ~375 ms and is dwarfed by the standing 5.3 mA.
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

/* Register addresses (register number == Modbus address on this probe) */
#define REG_CONCENTRATION_TIME 1  /* UINT */
#define REG_CONCENTRATION      2  /* UINT32 (L at 2, H at 3) */
#define REG_TEMPERATURE        4  /* signed int8 in low byte */
#define REG_HUMIDITY           5  /* UINT */
#define REG_CONCENTRATION_DAY  17 /* UINT32 (L at 17, H at 18) */
#define REG_LIMIT              33 /* UINT, read-only here (no write support) */
#define REG_RECORD_INTERVAL    36 /* UINT, read-only here (no write support) */
#define REG_SPECTRUM_INTERVAL  37 /* UINT, read-only here (no write support) */
#define REG_ALGORITHM          38 /* UINT, read-only here (no write support) */
#define REG_IDENT_DEVICE       60 /* 10 ASCII */
#define REG_IDENT_VERSION      65 /* 10 ASCII */
#define REG_IDENT_SERIAL       70 /* 10 ASCII */

/* Polled for a device configured without an address */
#define DEFAULT_ADDR 1

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

static int init(struct app_driver_slot *slot)
{
	struct app_data_piketronic_rpp *data = slot->ctx;

	LOG_INF("Initializing Piketronic RPP-R driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_piketronic_rpp *data = slot->ctx;
	/* A device configured without an address reaches the driver as 0. The single-instance
	 * driver polled DEFAULT_ADDR for it, so keep doing that rather than sending a broadcast
	 * that no probe answers.
	 */
	uint8_t addr = slot->addr ? slot->addr : DEFAULT_ADDR;
	int ret;
	int errors = 0;
	uint16_t regs[16];

	uint32_t concentration = 0, concentration_day = 0;
	int8_t temperature = 0;
	uint8_t humidity = 0;
	uint16_t concentration_time = 0;
	uint16_t limit = 0, record_interval = 0, spectrum_interval = 0, algorithm = 0;
	char device[11] = {0}, version_sw[11] = {0}, serial_number[11] = {0};

	LOG_INF("Sampling Piketronic RPP-R at address %d", addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	/* Measurements: regs 1-5 (time, concentration L+H, temperature, humidity) */
	if (app_modbus_read_holding_regs(addr, REG_CONCENTRATION_TIME, 5, regs) == 0) {
		concentration_time = regs[0];
		concentration = app_modbus_decode_u32(
			&regs[1], APP_MODBUS_WO_LO_FIRST); /* L at reg 2, H at reg 3 */
		temperature = (int8_t)(regs[3] & 0xFF);
		humidity = (uint8_t)(regs[4] & 0xFF);
	} else {
		errors++;
	}

	/* Daily average: regs 17-18 */
	if (app_modbus_read_holding_regs(addr, REG_CONCENTRATION_DAY, 2, regs) == 0) {
		concentration_day = app_modbus_decode_u32(regs, APP_MODBUS_WO_LO_FIRST);
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

	app_modbus_release();

	if (errors) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("RPP-R: %d read errors", errors);
		return -EIO;
	}

	data->concentration = concentration;
	data->concentration_day = concentration_day;
	data->temperature = temperature;
	data->humidity = humidity;
	data->concentration_time = concentration_time;
	data->limit = limit;
	data->record_interval = record_interval;
	data->spectrum_interval = spectrum_interval;
	data->algorithm = algorithm;
	strcpy(data->device, device);
	strcpy(data->version_sw, version_sw);
	strcpy(data->serial_number, serial_number);
	data->valid = true;
	data->last_sample = k_uptime_get_32();
	data->error_count = 0;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct piketronic_rpp_sample rec = {
		.timestamp = ts,
		.concentration = concentration,
		.concentration_day = concentration_day,
		.temperature = temperature,
		.humidity = humidity,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("RPP-R: Rn=%u Bq/m3, T=%d C, RH=%u %%", concentration, temperature, humidity);

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

	int ret = app_device_probe_sample(shell, &piketronic_rpp_driver, addr);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Address and baud/parity are DIP switches on the probe */
	shell_print(shell, "RPP-R: the address is set by DIP switches on the probe");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	return 0;
}

static const struct shell_static_entry piketronic_rpp_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address: config <addr>", cmd_config, 1, 1),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry piketronic_rpp_shell_cmds = {
	.entry = piketronic_rpp_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_piketronic_rpp *d = slot->ctx;

	app_device_print_header(shell, slot, "Piketronic RPP-R");
	shell_print(shell, "  radon (1h avg):   %u Bq/m3", d->concentration);
	shell_print(shell, "  radon (1d avg):   %u Bq/m3", d->concentration_day);
	shell_print(shell, "  temperature:      %d C", d->temperature);
	shell_print(shell, "  humidity:         %u %%", d->humidity);
	shell_print(shell, "  interval timer:   %u s (into 4-min cycle)", d->concentration_time);
	shell_print(shell, "  alarm limit:      %u Bq/m3", d->limit);
	shell_print(shell, "  record interval:  %u min", d->record_interval);
	shell_print(shell, "  spectrum interval:%u min", d->spectrum_interval);
	shell_print(shell, "  calc mode:        %u (%s)", d->algorithm,
		    d->algorithm == 0 ? "RnA" : "RnA+RnC");
	shell_print(shell, "  device:           %s", d->device);
	shell_print(shell, "  version:          %s", d->version_sw);
	shell_print(shell, "  serial:           %s", d->serial_number);
}

const struct app_device_driver piketronic_rpp_driver = {
	.name = "piketronic",
	.type = APP_DEVICE_TYPE_PIKETRONIC_RPP,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_piketronic_rpp),
	.rec_size = sizeof(struct piketronic_rpp_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_piketronic_rpp *piketronic_rpp_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
