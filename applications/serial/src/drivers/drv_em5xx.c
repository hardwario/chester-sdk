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

/* EM5XX Register addresses (INT32 LSW) */
#define REG_CURRENT           0x00F8  /* /1000 A */
#define REG_POWER             0x0106  /* /10000 kW */
#define REG_APPARENT          0x0108  /* /10000 kVA, sum */
#define REG_REACTIVE          0x010A  /* /10000 kvar, sum */
#define REG_PF                0x010C  /* /1000 PF, sum */
#define REG_FREQUENCY         0x0110  /* /10 Hz */
#define REG_ENERGY_IN         0x0112  /* /10 kWh */
#define REG_ENERGY_OUT        0x0116  /* /10 kWh */
#define REG_VOLTAGE_L1        0x0120  /* /10 V */
#define REG_CURRENT_L1        0x0122  /* /1000 A */
#define REG_POWER_L1          0x0124  /* /10000 kW */
#define REG_PF_L1             0x012A  /* /1000 PF L1 */
#define REG_VOLTAGE_L2        0x012E  /* /10 V */
#define REG_CURRENT_L2        0x0130  /* /1000 A */
#define REG_POWER_L2          0x0132  /* /10000 kW */
#define REG_PF_L2             0x0138  /* /1000 PF L2 */
#define REG_VOLTAGE_L3        0x013C  /* /10 V */
#define REG_CURRENT_L3        0x013E  /* /1000 A */
#define REG_POWER_L3          0x0140  /* /10000 kW */
#define REG_PF_L3             0x0146  /* /1000 PF L3 */

/* clang-format off */
/* Total values (system voltage average is intentionally not read — it's misleading on a
 * 1-phase test rig; per-phase voltage_l1/l2/l3 carry the actual measurements)
 */
static const struct app_device_reg m_regs[] = {
	APP_DEVICE_REG(struct app_data_em5xx, current, REG_CURRENT, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power, REG_POWER, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_apparent, REG_APPARENT, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_reactive, REG_REACTIVE, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_factor, REG_PF, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, frequency, REG_FREQUENCY, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, energy_in, REG_ENERGY_IN, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, energy_out, REG_ENERGY_OUT, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, voltage_l1, REG_VOLTAGE_L1, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, current_l1, REG_CURRENT_L1, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_l1, REG_POWER_L1, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_factor_l1, REG_PF_L1, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, voltage_l2, REG_VOLTAGE_L2, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, current_l2, REG_CURRENT_L2, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_l2, REG_POWER_L2, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_factor_l2, REG_PF_L2, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, voltage_l3, REG_VOLTAGE_L3, APP_MODBUS_S32_LO, 10.0f),
	APP_DEVICE_REG(struct app_data_em5xx, current_l3, REG_CURRENT_L3, APP_MODBUS_S32_LO, 1000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_l3, REG_POWER_L3, APP_MODBUS_S32_LO, 10000.0f),
	APP_DEVICE_REG(struct app_data_em5xx, power_factor_l3, REG_PF_L3, APP_MODBUS_S32_LO, 1000.0f),
};
/* clang-format on */

static int init(struct app_driver_slot *slot)
{
	struct app_data_em5xx *data = slot->ctx;

	LOG_INF("Initializing EM5XX driver");
	data->valid = false;
	data->error_count = 0;
	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_em5xx *data = slot->ctx;
	struct app_data_em5xx values = *data;
	int ret;

	LOG_INF("Sampling EM5XX at address %d", slot->addr);

	ret = app_modbus_acquire();
	if (ret) {
		LOG_ERR("Failed to enable Modbus: %d", ret);
		data->error_count++;
		return ret;
	}

	/* All values are read; the data is updated only if every read succeeded */
	ret = app_device_read_regs(slot->addr, APP_MODBUS_FC_HOLDING, m_regs, ARRAY_SIZE(m_regs),
				   &values, false);

	app_modbus_release();

	if (ret) {
		data->valid = false;
		data->error_count++;
		LOG_WRN("EM5XX: read errors");
		return -EIO;
	}

	values.valid = true;
	values.last_sample = k_uptime_get_32();
	values.error_count = 0;
	*data = values;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct em5xx_sample rec = {
		.timestamp = ts,
		.current = data->current,
		.power = data->power,
		.power_apparent = data->power_apparent,
		.power_reactive = data->power_reactive,
		.power_factor = data->power_factor,
		.frequency = data->frequency,
		.energy_in = data->energy_in,
		.energy_out = data->energy_out,
		.voltage_l1 = data->voltage_l1,
		.voltage_l2 = data->voltage_l2,
		.voltage_l3 = data->voltage_l3,
		.current_l1 = data->current_l1,
		.current_l2 = data->current_l2,
		.current_l3 = data->current_l3,
		.power_l1 = data->power_l1,
		.power_l2 = data->power_l2,
		.power_l3 = data->power_l3,
		.power_factor_l1 = data->power_factor_l1,
		.power_factor_l2 = data->power_factor_l2,
		.power_factor_l3 = data->power_factor_l3,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("EM5XX: I=%.3f A, P=%.4f kW, E_in=%.1f kWh", (double)data->current,
		(double)data->power, (double)data->energy_in);

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

	int ret = app_device_probe_sample(shell, &em5xx_driver, addr);
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

	shell_print(shell, "EM5XX: address and parity are set on the meter or with its software");
	shell_print(shell, "Set the address in the device configuration: app config device-N");
	shell_print(shell, "Set the matching parity: app config serial-parity");
	return 0;
}

static const struct shell_static_entry em5xx_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read values: sample <addr>", cmd_sample, 2, 0),
	SHELL_CMD_ARG(config, NULL, "How to set the address and parity", cmd_config, 1, 2),
	SHELL_SUBCMD_SET_END};

const union shell_cmd_entry em5xx_shell_cmds = {
	.entry = em5xx_subcmds
};

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_em5xx *data = slot->ctx;

	app_device_print_header(shell, slot, "EM5XX (EM540)");
	shell_print(shell, "  voltage_l1:      %.1f V", (double)data->voltage_l1);
	shell_print(shell, "  voltage_l2:      %.1f V", (double)data->voltage_l2);
	shell_print(shell, "  voltage_l3:      %.1f V", (double)data->voltage_l3);
	shell_print(shell, "  current:         %.3f A", (double)data->current);
	shell_print(shell, "  current_l1:      %.3f A", (double)data->current_l1);
	shell_print(shell, "  current_l2:      %.3f A", (double)data->current_l2);
	shell_print(shell, "  current_l3:      %.3f A", (double)data->current_l3);
	shell_print(shell, "  frequency:       %.2f Hz", (double)data->frequency);
	shell_print(shell, "  power:           %.4f kW", (double)data->power);
	shell_print(shell, "  power_l1:        %.4f kW", (double)data->power_l1);
	shell_print(shell, "  power_l2:        %.4f kW", (double)data->power_l2);
	shell_print(shell, "  power_l3:        %.4f kW", (double)data->power_l3);
	shell_print(shell, "  power_apparent:  %.4f kVA", (double)data->power_apparent);
	shell_print(shell, "  power_reactive:  %.4f kvar", (double)data->power_reactive);
	shell_print(shell, "  power_factor:    %.3f", (double)data->power_factor);
	shell_print(shell, "  power_factor_l1: %.3f", (double)data->power_factor_l1);
	shell_print(shell, "  power_factor_l2: %.3f", (double)data->power_factor_l2);
	shell_print(shell, "  power_factor_l3: %.3f", (double)data->power_factor_l3);
	shell_print(shell, "  energy_in:       %.3f kWh", (double)data->energy_in);
	shell_print(shell, "  energy_out:      %.3f kWh", (double)data->energy_out);
}

const struct app_device_driver em5xx_driver = {
	.name = "em5xx",
	.type = APP_DEVICE_TYPE_EM5XX,
	.transport = APP_DEVICE_TRANSPORT_MODBUS,
	.ctx_size = sizeof(struct app_data_em5xx),
	.rec_size = sizeof(struct em5xx_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

const struct app_data_em5xx *em5xx_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}
