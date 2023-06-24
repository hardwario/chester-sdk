/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_BATT_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_BATT_H_

/* Zephyr includes */
#include <zephyr/device.h>

#define CTR_BATT_REST_TIMEOUT_DEFAULT_MS 1000
#define CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS 9000

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ctr_batt_api_get_rest_voltage_mv)(const struct device *dev, int *rest_mv,
						int delay_ms);
typedef int (*ctr_batt_api_get_load_voltage_mv)(const struct device *dev, int *load_mv,
						int delay_ms);
typedef void (*ctr_batt_api_get_load_current_ma)(const struct device *dev, int *current_ma,
						 int load_mv);
typedef int (*ctr_batt_api_load)(const struct device *dev);
typedef int (*ctr_batt_api_unload)(const struct device *dev);

struct ctr_batt_driver_api {
	ctr_batt_api_get_rest_voltage_mv get_rest_voltage_mv;
	ctr_batt_api_get_load_voltage_mv get_load_voltage_mv;
	ctr_batt_api_get_load_current_ma get_load_current_ma;
	ctr_batt_api_load load;
	ctr_batt_api_unload unload;
};

static inline int ctr_batt_get_rest_voltage_mv(const struct device *dev, int *rest_mv, int delay_ms)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->get_rest_voltage_mv(dev, rest_mv, delay_ms);
}

static inline int ctr_batt_get_load_voltage_mv(const struct device *dev, int *load_mv, int delay_ms)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->get_load_voltage_mv(dev, load_mv, delay_ms);
}

static inline void ctr_batt_get_load_current_ma(const struct device *dev, int *current_ma,
						int load_mv)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	api->get_load_current_ma(dev, current_ma, load_mv);
}

static inline int ctr_batt_load(const struct device *dev)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->load(dev);
}

static inline int ctr_batt_unload(const struct device *dev)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->unload(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_BATT_H_ */
