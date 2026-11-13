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

/* EM1XX Register addresses (INT32 LSW) */
#define REG_CURRENT       0x0100  /* /1000 A */
#define REG_VOLTAGE       0x0102  /* /10 V */
#define REG_POWER         0x0106  /* /10000 kW */
#define REG_APPARENT      0x0108  /* /10000 kVA */
#define REG_REACTIVE      0x010A  /* /10000 kvar */
#define REG_PF            0x010C  /* /1000 PF */
#define REG_FREQUENCY     0x0110  /* /10 Hz */
#define REG_ENERGY_IN     0x0112  /* /10 kWh */
#define REG_ENERGY_OUT    0x0116  /* /10 kWh */

/* clang-format off */
static const struct app_device_reg m_regs[] = {
	APP_DEVICE_REG(struct app_data_em1xx, current, REG_CURRENT, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em1xx, voltage, REG_VOLTAGE, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em1xx, power, REG_POWER, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em1xx, power_apparent, REG_APPARENT, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em1xx, power_reactive, REG_REACTIVE, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em1xx, power_factor, REG_PF, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em1xx, frequency, REG_FREQUENCY, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em1xx, energy_in, REG_ENERGY_IN, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em1xx, energy_out, REG_ENERGY_OUT, APP_MODBUS_S32_LO, 10.0f),
};
/* clang-format on */

static int init(struct app_driver_slot *slot)
{
	struct app_data_em1xx *data = slot->ctx;

	LOG_INF("Initializing EM1XX driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_em1xx *data = slot->ctx;
	struct app_data_em1xx values = *data;
	int ret;

	LOG_INF("Sampling EM1XX at address %d", slot->addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	ret = app_device_read_regs(slot->addr, APP_MODBUS_FC_HOLDING, m_regs, ARRAY_SIZE(m_regs),
				   &values, true);

	app_modbus_release();

	if (ret) {
		data->error_count++;
		data->valid = false;
		return ret;
	}

	values.valid = true;
	values.last_sample = k_uptime_get_32();
	values.error_count = 0;
	*data = values;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct em1xx_sample rec = {
		.timestamp = ts,
		.voltage = data->voltage,
		.current = data->current,
		.power = data->power,
		.power_apparent = data->power_apparent,
		.power_reactive = data->power_reactive,
		.power_factor = data->power_factor,
		.frequency = data->frequency,
		.energy_in = data->energy_in,
		.energy_out = data->energy_out,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("EM1XX: V=%.1f V, I=%.3f A, P=%.4f kW, F=%.1f Hz, E_in=%.1f kWh, E_out=%.1f kWh",
		(double)data->voltage, (double)data->current, (double)data->power,
		(double)data->frequency, (double)data->energy_in, (double)data->energy_out);

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

	int ret = app_device_probe_sample(shell, &em1xx_driver, addr);
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

	shell_print(shell, "EM1XX: address and parity are set on the meter or with its software");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	shell_print(shell, "Set the matching parity: app config serial-parity");
	return 0;
}

static const struct shell_static_entry em1xx_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address and parity", cmd_config, 1, 3),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry em1xx_shell_cmds = {
	.entry = em1xx_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_em1xx *data = slot->ctx;

	app_device_print_header(shell, slot, "EM1XX (EM111)");
	shell_print(shell, "  voltage:        %.1f V", (double)data->voltage);
	shell_print(shell, "  current:        %.3f A", (double)data->current);
	shell_print(shell, "  frequency:      %.2f Hz", (double)data->frequency);
	shell_print(shell, "  power:          %.4f kW", (double)data->power);
	shell_print(shell, "  power_apparent: %.4f kVA", (double)data->power_apparent);
	shell_print(shell, "  power_reactive: %.4f kvar", (double)data->power_reactive);
	shell_print(shell, "  power_factor:   %.3f", (double)data->power_factor);
	shell_print(shell, "  energy_in:      %.3f kWh", (double)data->energy_in);
	shell_print(shell, "  energy_out:     %.3f kWh", (double)data->energy_out);
}

const struct app_device_driver em1xx_driver = {
	.name = "em1xx",
	.type = APP_DEVICE_TYPE_EM1XX,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_em1xx),
	.rec_size = sizeof(struct em1xx_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_em1xx *em1xx_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
