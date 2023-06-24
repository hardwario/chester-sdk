/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_RFMUX_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_RFMUX_H_

/* Zephyr includes */
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_rfmux_interface {
	CTR_RFMUX_INTERFACE_NONE = 0,
	CTR_RFMUX_INTERFACE_LTE = 1,
	CTR_RFMUX_INTERFACE_LRW = 2,
};

enum ctr_rfmux_antenna {
	CTR_RFMUX_ANTENNA_NONE = 0,
	CTR_RFMUX_ANTENNA_INT = 1,
	CTR_RFMUX_ANTENNA_EXT = 2,
};

typedef int (*ctr_rfmux_api_acquire)(const struct device *dev);
typedef int (*ctr_rfmux_api_release)(const struct device *dev);
typedef int (*ctr_rfmux_api_set_interface)(const struct device *dev,
					   enum ctr_rfmux_interface interface);
typedef int (*ctr_rfmux_api_set_antenna)(const struct device *dev, enum ctr_rfmux_antenna antenna);

struct ctr_rfmux_driver_api {
	ctr_rfmux_api_acquire acquire;
	ctr_rfmux_api_release release;
	ctr_rfmux_api_set_interface set_interface;
	ctr_rfmux_api_set_antenna set_antenna;
};

static inline int ctr_rfmux_acquire(const struct device *dev)
{
	const struct ctr_rfmux_driver_api *api = (const struct ctr_rfmux_driver_api *)dev->api;

	return api->acquire(dev);
}

static inline int ctr_rfmux_release(const struct device *dev)
{
	const struct ctr_rfmux_driver_api *api = (const struct ctr_rfmux_driver_api *)dev->api;

	return api->release(dev);
}

static inline int ctr_rfmux_set_interface(const struct device *dev,
					  enum ctr_rfmux_interface interface)
{
	const struct ctr_rfmux_driver_api *api = (const struct ctr_rfmux_driver_api *)dev->api;

	return api->set_interface(dev, interface);
}

static inline int ctr_rfmux_set_antenna(const struct device *dev, enum ctr_rfmux_antenna antenna)
{
	const struct ctr_rfmux_driver_api *api = (const struct ctr_rfmux_driver_api *)dev->api;

	return api->set_antenna(dev, antenna);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_RFMUX_H_ */
