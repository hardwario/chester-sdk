/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X3_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X3_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x3_channel {
	CTR_X3_CHANNEL_1 = 0,
	CTR_X3_CHANNEL_2 = 1,
};

typedef int (*ctr_x3_api_set_power)(const struct device *dev, enum ctr_x3_channel channel,
				    bool enabled);
typedef int (*ctr_x3_api_measure)(const struct device *dev, enum ctr_x3_channel channel,
				  int32_t *result);

struct ctr_x3_driver_api {
	ctr_x3_api_set_power set_power;
	ctr_x3_api_measure measure;
};

static inline int ctr_x3_set_power(const struct device *dev, enum ctr_x3_channel channel,
				   bool enabled)
{
	const struct ctr_x3_driver_api *api = (const struct ctr_x3_driver_api *)dev->api;

	return api->set_power(dev, channel, enabled);
}

static inline int ctr_x3_measure(const struct device *dev, enum ctr_x3_channel channel,
				 int32_t *result)
{
	const struct ctr_x3_driver_api *api = (const struct ctr_x3_driver_api *)dev->api;

	return api->measure(dev, channel, result);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X3_H_ */
