#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(app);

void main(void)
{
	LOG_INF("Hello world");

	while (1) {
		LOG_INF("Alive");
		k_sleep(K_MSEC(100));
	}
}
