/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_batt.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_hygro.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		k_sleep(K_MSEC(500));
	}
}
