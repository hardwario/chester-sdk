/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_gpio.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define CONFIGURE_GPIO(channel)                                                                    \
	do {                                                                                       \
		int ret;                                                                           \
		ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_##channel, CTR_GPIO_MODE_OUTPUT);         \
		if (ret) {                                                                         \
			LOG_ERR("Failed initializing channel " Z_STRINGIFY(channel) ": %d", ret);  \
			k_oops();                                                                  \
		}                                                                                  \
	} while (0)

#define PULSE_GPIO(channel)                                                                        \
	do {                                                                                       \
		int ret;                                                                           \
		LOG_DBG("Writing log. 1 to channel " Z_STRINGIFY(channel));                        \
		ret = ctr_gpio_write(CTR_GPIO_CHANNEL_##channel, 1);                               \
		if (ret) {                                                                         \
			LOG_ERR("Failed writing log. 1 to channel " STRINGIFY(channel) ": %d",     \
									      ret);                \
			k_oops();                                                                  \
		}                                                                                  \
		k_sleep(K_MSEC(500));                                                              \
		LOG_DBG("Writing log. 0 to channel " Z_STRINGIFY(channel));                        \
		ret = ctr_gpio_write(CTR_GPIO_CHANNEL_##channel, 0);                               \
		if (ret) {                                                                         \
			LOG_ERR("Failed writing log. 0 to channel " Z_STRINGIFY(channel) ": %d",   \
										ret);              \
			k_oops();                                                                  \
		}                                                                                  \
	} while (0)

int main(void)
{
	CONFIGURE_GPIO(A0);
	CONFIGURE_GPIO(A1);
	CONFIGURE_GPIO(A2);
	CONFIGURE_GPIO(A3);
	CONFIGURE_GPIO(B0);
	CONFIGURE_GPIO(B1);
	CONFIGURE_GPIO(B2);
	CONFIGURE_GPIO(B3);

	for (;;) {
		PULSE_GPIO(A0);
		PULSE_GPIO(A1);
		PULSE_GPIO(A2);
		PULSE_GPIO(A3);
		PULSE_GPIO(B0);
		PULSE_GPIO(B1);
		PULSE_GPIO(B2);
		PULSE_GPIO(B3);
	}

	return 0;
}
