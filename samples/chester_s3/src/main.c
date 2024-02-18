/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/drivers/ctr_s3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void ctr_s3_handler(const struct device *dev, enum ctr_s3_event event, void *user_data)
{
	switch (event) {
	case CTR_S3_EVENT_MOTION_L_DETECTED:
		LOG_INF("Motion L detected");
		ctr_led_set(CTR_LED_CHANNEL_R, false);
		k_sleep(K_MSEC(50));
		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(50));
		ctr_led_set(CTR_LED_CHANNEL_G, false);
		break;
	case CTR_S3_EVENT_MOTION_R_DETECTED:
		LOG_INF("Motion R detected");
		ctr_led_set(CTR_LED_CHANNEL_G, false);
		k_sleep(K_MSEC(50));
		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(50));
		ctr_led_set(CTR_LED_CHANNEL_R, false);
		break;
	default:
		LOG_ERR("Unknown event: %d", event);
		return;
	}
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s3));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_s3_set_handler(dev, ctr_s3_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_s3_set_handler` failed: %d", ret);
		k_oops();
	}

	struct ctr_s3_param param = {
		.sensitivity = 32,
		.blind_time = 1,
		.pulse_counter = 1,
		.window_time = 0,
	};

	ret = ctr_s3_configure(dev, CTR_S3_CHANNEL_L, &param);
	if (ret) {
		LOG_ERR("Call `ctr_s3_configure` failed: %d", ret);
		k_oops();
	}

	ret = ctr_s3_configure(dev, CTR_S3_CHANNEL_R, &param);
	if (ret) {
		LOG_ERR("Call `ctr_s3_configure` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
