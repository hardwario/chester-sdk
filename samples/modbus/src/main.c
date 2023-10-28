/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static int iface;

static void init_modbus(void)
{
	int ret;

	const char iface_name[] = {
		DEVICE_DT_NAME(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)),
	};

	iface = modbus_iface_get_by_name(iface_name);

	const struct modbus_iface_param client_param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = 500000,
		.serial =
			{
				.baud = 9600,
				.parity = UART_CFG_PARITY_NONE,
				.stop_bits_client = UART_CFG_STOP_BITS_1,
			},
	};

	ret = modbus_init_client(iface, client_param);

	if (ret < 0) {
		LOG_ERR("Call `modbus_init_client` failed: %d", ret);
		k_oops();
	}
}

static void read_modbus(void)
{
	int ret;
	uint16_t reg[1];

	ret = modbus_read_input_regs(iface, 1, 1, reg, ARRAY_SIZE(reg));

	if (ret != 0) {
		LOG_ERR("Call `modbus_read_input_regs` failed: %d", ret);
		return;
	}

	LOG_HEXDUMP_INF(reg, sizeof(reg), "Input register:");
}

int main(void)
{
	init_modbus();

	for (;;) {
		LOG_INF("Alive");
		read_modbus();
		k_sleep(K_SECONDS(30));
	}

	return 0;
}
