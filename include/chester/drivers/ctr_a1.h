/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_A1_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_A1_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_a1_relay {
	CTR_A1_RELAY_1 = 0,
	CTR_A1_RELAY_2 = 1,
};

enum ctr_a1_led {
	CTR_A1_LED_1 = 0,
	CTR_A1_LED_2 = 1,
};

typedef int (*ctr_a1_api_set_relay)(const struct device *dev, enum ctr_a1_relay relay, bool is_on);
typedef int (*ctr_a1_api_set_led)(const struct device *dev, enum ctr_a1_led led, bool is_on);

struct ctr_a1_driver_api {
	ctr_a1_api_set_relay set_relay;
	ctr_a1_api_set_led set_led;
};

static inline int ctr_a1_set_relay(const struct device *dev, enum ctr_a1_relay relay, bool is_on)
{
	const struct ctr_a1_driver_api *api = (const struct ctr_a1_driver_api *)dev->api;

	return api->set_relay(dev, relay, is_on);
}

static inline int ctr_a1_set_led(const struct device *dev, enum ctr_a1_led led, bool is_on)
{
	const struct ctr_a1_driver_api *api = (const struct ctr_a1_driver_api *)dev->api;

	return api->set_led(dev, led, is_on);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_A1_H_ */
