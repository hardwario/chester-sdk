/* Sensor wiring:
 * | A2 | CH1 | Sensor power  |
 * | A3 | GND | Sensor ground |
 * | A4 | CH2 | Sensor signal |
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <chester/ctr_led.h>

#include <chester/drivers/ctr_x0.h>
#include <chester/drivers/mb7066.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *mb7066 = DEVICE_DT_GET(DT_NODELABEL(mb7066));

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (mb7066 == NULL) {
		LOG_ERR("Could not get mb7066");
		k_oops();
	}

	if (!device_is_ready(mb7066)) {
		LOG_ERR("mb7066 is not ready");
		k_oops();
	}

	for (;;) {
		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(30));
		ctr_led_set(CTR_LED_CHANNEL_G, false);

		float val;
		mb7066_measure(mb7066, &val);
		LOG_INF("Measured: %d mm", (int)(val * 1000));

		k_sleep(K_MSEC(3000));
	}
}
