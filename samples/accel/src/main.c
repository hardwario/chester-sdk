/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_led.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void ctr_accel_event_handler(enum ctr_accel_event event, void *user_data)
{
	LOG_INF("Event: %d", event);
}

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_accel_set_event_handler(ctr_accel_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_accel_set_event_handler` failed: %d", ret);
		k_oops();
	}

	ret = ctr_accel_set_threshold(12.5, 5);
	if (ret) {
		LOG_ERR("Call `ctr_accel_set_threshold` failed: %d", ret);
		k_oops();
	}

	ret = ctr_accel_enable_trigger();
	if (ret) {
		LOG_ERR("Call `ctr_accel_enable_trigger` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		float accel_x;
		float accel_y;
		float accel_z;
		int orientation;
		ret = ctr_accel_read(&accel_x, &accel_y, &accel_z, &orientation);
		if (ret) {
			LOG_ERR("Call `ctr_accel_read` failed: %d", ret);

		} else {
			LOG_INF("Acceleration X: %.3f m/s^2", accel_x);
			LOG_INF("Acceleration Y: %.3f m/s^2", accel_y);
			LOG_INF("Acceleration Z: %.3f m/s^2", accel_z);
			LOG_INF("Orientation: %d", orientation);
		}

		k_sleep(K_SECONDS(5));
	}
}
