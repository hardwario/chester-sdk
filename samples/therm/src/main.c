/* CHESTER includes */
#include <ctr_led.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		float temperature;
		ret = ctr_therm_read(&temperature);
		if (ret) {
			LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		} else {
			LOG_INF("Temperature: %.2f C", temperature);
		}

		k_sleep(K_SECONDS(5));
	}
}
