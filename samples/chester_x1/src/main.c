/* CHESTER includes */
#include <chester/ctr_gpio.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_A0, CTR_GPIO_MODE_OUTPUT);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_A0, 1);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
		k_oops();
	}

	/*
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x1_ds2482_a));

	if (!device_is_ready(dev)) {
	        LOG_ERR("Device not ready");
	        k_oops();
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
	        LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
	        k_oops();
	}
	*/

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
