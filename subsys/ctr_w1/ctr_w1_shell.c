/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_w1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_w1_shell, CONFIG_CTR_W1_LOG_LEVEL);

struct scan_shell_data {
	const struct shell *shell;
	int count;
};

static int scan_callback(struct w1_rom rom, void *user_data)
{
	struct scan_shell_data *data = user_data;

	data->count++;

	uint64_t serial = sys_get_le48(rom.serial);

	shell_print(data->shell, "[%d] family: 0x%02x  serial: 0x%012llx  rom: 0x%016llx",
		    data->count, rom.family, (unsigned long long)serial,
		    (unsigned long long)w1_rom_to_uint64(&rom));

	return 0;
}

static int cmd_w1_scan(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	struct ctr_w1 w1;

	ret = ctr_w1_acquire(&w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	struct scan_shell_data data = {
		.shell = shell,
		.count = 0,
	};

	ret = ctr_w1_scan(&w1, dev, scan_callback, &data);
	if (ret) {
		LOG_ERR("Call `ctr_w1_scan` failed: %d", ret);
		shell_error(shell, "command failed");
		ctr_w1_release(&w1, dev);
		return ret;
	}

	ret = ctr_w1_release(&w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "found %d device(s)", data.count);
	shell_print(shell, "command succeeded");

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_w1,

	SHELL_CMD_ARG(scan, NULL,
	              "Scan 1-Wire bus for devices.",
	              cmd_w1_scan, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(w1, &sub_w1, "1-Wire commands.", print_help);

/* clang-format on */
