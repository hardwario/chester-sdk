/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_MB7066_H_
#define CHESTER_INCLUDE_DRIVERS_MB7066_H_

/* Zephyr includes */
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mb7066_api_measure)(const struct device *dev, float *value);

struct mb7066_driver_api {
	mb7066_api_measure measure;
};

static inline int mb7066_measure(const struct device *dev, float *value)
{
	const struct mb7066_driver_api *api = dev->api;
	return api->measure(dev, value);
}

#ifdef __cplusplus
}
#endif

#endif
