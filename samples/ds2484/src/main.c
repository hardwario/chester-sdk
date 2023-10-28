/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void search_callback(struct w1_rom rom, void *cb_arg)
{
	char serial_buf[13] = {0};

	size_t serial_buf_len =
		bin2hex(rom.serial, sizeof(rom.serial), serial_buf, sizeof(serial_buf));

	LOG_INF("Device found; family: 0x%x, serial: 0x%.*s", rom.family, serial_buf_len,
		serial_buf);
}

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	const struct device *dev_ds2484 = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	for (;;) {
		w1_lock_bus(dev_ds2484);
		pm_device_action_run(dev_ds2484, PM_DEVICE_ACTION_RESUME);
		size_t num_devices = w1_search_rom(dev_ds2484, search_callback, NULL);
		pm_device_action_run(dev_ds2484, PM_DEVICE_ACTION_SUSPEND);
		w1_unlock_bus(dev_ds2484);

		LOG_INF("Number of devices found on bus: %u", num_devices);

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
