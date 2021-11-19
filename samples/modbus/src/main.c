/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <modbus/modbus.h>
#include <sys/byteorder.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static int iface;

static void init_modbus(void)
{
	int ret;

	const char iface_name[] = {
		DT_PROP(DT_INST(0, zephyr_modbus_serial), label),
	};

	iface = modbus_iface_get_by_name(iface_name);

	const struct modbus_iface_param client_param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = 50000,
		.serial = {
			.baud = 9600,
			.parity = UART_CFG_PARITY_NONE,
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
		LOG_ERR("Call `modbus_read_holding_regs` failed: %d", ret);
		return;
	}

	LOG_HEXDUMP_INF(reg, sizeof(reg), "Input register:");
}

void main(void)
{
	k_sleep(K_SECONDS(3));
	init_modbus();

	for (;;) {
		LOG_INF("Alive");
		read_modbus();
		k_sleep(K_SECONDS(30));
	}
}
