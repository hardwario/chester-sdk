/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_M8_H_
#define CHESTER_INCLUDE_DRIVERS_M8_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*m8_api_set_main_power)(const struct device *dev, bool on);
typedef int (*m8_api_set_bckp_power)(const struct device *dev, bool on);
typedef int (*m8_api_read_buffer)(const struct device *dev, void *buf, size_t buf_size,
				  size_t *bytes_read);

struct m8_driver_api {
	m8_api_set_main_power set_main_power;
	m8_api_set_bckp_power set_bckp_power;
	m8_api_read_buffer read_buffer;
};

static inline int m8_set_main_power(const struct device *dev, bool on)
{
	const struct m8_driver_api *api = (const struct m8_driver_api *)dev->api;

	return api->set_main_power(dev, on);
}

static inline int m8_set_bckp_power(const struct device *dev, bool on)
{
	const struct m8_driver_api *api = (const struct m8_driver_api *)dev->api;

	return api->set_bckp_power(dev, on);
}

static inline int m8_read_buffer(const struct device *dev, void *buf, size_t buf_size,
				 size_t *bytes_read)
{
	const struct m8_driver_api *api = (const struct m8_driver_api *)dev->api;

	return api->read_buffer(dev, buf, buf_size, bytes_read);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_M8_H_ */
