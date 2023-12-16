/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LED_H_
#define CHESTER_INCLUDE_CTR_LED_H_

#include <zephyr/drivers/led.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_led_channel {
	CTR_LED_CHANNEL_R = 0,
	CTR_LED_CHANNEL_G = 1,
	CTR_LED_CHANNEL_Y = 2,
	CTR_LED_CHANNEL_EXT = 3,
	CTR_LED_CHANNEL_LOAD = 4,
};

static inline int ctr_led_set(enum ctr_led_channel channel, bool is_on)
{
	if (is_on) {
		return led_on(DEVICE_DT_GET(DT_NODELABEL(gpio_leds)), channel);
	} else {
		return led_off(DEVICE_DT_GET(DT_NODELABEL(gpio_leds)), channel);
	}
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LED_H_ */
