#include <device.h>
#include <devicetree.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(app);

void main(void)
{
	LOG_INF("Hello world");

	while (1) {
		LOG_INF("Alive");
		k_sleep(K_MSEC(100));
	}
}
