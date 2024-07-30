/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *pwm_leds = DEVICE_DT_GET(DT_NODELABEL(pwm_leds));

CTR_LED_SEQ_DEFINE(fade, CTR_LED_PRIO_LOW, false,
		   {
			   .value = CTR_LED_CHANNEL_G,
			   .length = 10000,
			   .fade = true,
		   },
		   {
			   .value = CTR_LED_CHANNEL_R,
		   });

CTR_LED_SEQ_DEFINE(blink, CTR_LED_PRIO_MEDIUM, true,
		   {
			   .value = 0,
			   .length = 500,
		   },
		   {
			   .value = CTR_LED_CHANNEL_Y,
			   .length = 50,
		   });

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	bool state = false;

	ctr_led_play(pwm_leds, fade);
	k_sleep(K_MSEC(2000));
	ctr_led_play(pwm_leds, blink);
	k_sleep(K_MSEC(4000));
	ctr_led_stop(pwm_leds, blink);

	for (;;) {
		LOG_INF("Alive");

		/* Invert state variable */
		state = !state;

		/* Control LED */
		ctr_led_fade(pwm_leds, CTR_LED_CHANNEL_R * state, CTR_LED_CHANNEL_R * !state,
			     K_MSEC(1000));
	}

	return 0;
}
