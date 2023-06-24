/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_METEO_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_METEO_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ctr_meteo_api_get_rainfall_and_clear)(const struct device *dev, float *rainfall_mm);
typedef int (*ctr_meteo_api_get_wind_speed_and_clear)(const struct device *dev,
						      float *wind_speed_mps);
typedef int (*ctr_meteo_api_get_wind_direction)(const struct device *dev, float *direction);

struct ctr_meteo_driver_api {
	ctr_meteo_api_get_rainfall_and_clear get_rainfall_and_clear;
	ctr_meteo_api_get_wind_speed_and_clear get_wind_speed_and_clear;
	ctr_meteo_api_get_wind_direction get_wind_direction;
};

static inline int ctr_meteo_get_rainfall_and_clear(const struct device *dev, float *rainfall_mm)
{
	const struct ctr_meteo_driver_api *api = (const struct ctr_meteo_driver_api *)dev->api;

	return api->get_rainfall_and_clear(dev, rainfall_mm);
}

static inline int ctr_meteo_get_wind_speed_and_clear(const struct device *dev,
						     float *wind_speed_mps)
{
	const struct ctr_meteo_driver_api *api = (const struct ctr_meteo_driver_api *)dev->api;

	return api->get_wind_speed_and_clear(dev, wind_speed_mps);
}

static inline int ctr_meteo_get_wind_direction(const struct device *dev, float *direction)
{
	const struct ctr_meteo_driver_api *api = (const struct ctr_meteo_driver_api *)dev->api;

	return api->get_wind_direction(dev, direction);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_METEO_H_ */
