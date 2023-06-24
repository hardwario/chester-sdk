/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_GPIO_H_
#define CHESTER_INCLUDE_CTR_GPIO_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_gpio_channel {
	CTR_GPIO_CHANNEL_A0 = 0,
	CTR_GPIO_CHANNEL_A1 = 1,
	CTR_GPIO_CHANNEL_A2 = 2,
	CTR_GPIO_CHANNEL_A3 = 3,
	CTR_GPIO_CHANNEL_B0 = 4,
	CTR_GPIO_CHANNEL_B1 = 5,
	CTR_GPIO_CHANNEL_B2 = 6,
	CTR_GPIO_CHANNEL_B3 = 7,
};

enum ctr_gpio_mode {
	CTR_GPIO_MODE_ANALOG = 0,
	CTR_GPIO_MODE_OUTPUT = 1,
	CTR_GPIO_MODE_OUTPUT_OD = 2,
	CTR_GPIO_MODE_INPUT = 3,
	CTR_GPIO_MODE_INPUT_PU = 4,
	CTR_GPIO_MODE_INPUT_PD = 5,
};

int ctr_gpio_set_mode(enum ctr_gpio_channel channel, enum ctr_gpio_mode mode);
int ctr_gpio_read(enum ctr_gpio_channel channel, int *value);
int ctr_gpio_write(enum ctr_gpio_channel channel, int value);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_GPIO_H_ */
