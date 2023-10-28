/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_button.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void handler(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
		    void *user_data)
{
	LOG_INF("Event: %d, %d, %d", chan, ev, val);

	ctr_led_set(CTR_LED_CHANNEL_Y, 1);
	k_sleep(K_MSEC(30));
	ctr_led_set(CTR_LED_CHANNEL_Y, 0);
}

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ctr_button_set_event_cb(handler, NULL);

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_G, 1);
		k_sleep(K_MSEC(30));
		ctr_led_set(CTR_LED_CHANNEL_G, 0);

		k_sleep(K_MSEC(5000));
	}

	return 0;
}
