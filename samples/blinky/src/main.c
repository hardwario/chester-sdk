/* CHESTER includes */
#include <ctr_led.h>

/* Zephyr includes */
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	bool state = false;

	for (;;) {
		LOG_INF("Alive");

		/* Invert state variable */
		state = !state;

		/* Control LED */
		ctr_led_set(CTR_LED_CHANNEL_R, state);

		/* Wait 500 ms */
		k_sleep(K_MSEC(500));
	}
}
