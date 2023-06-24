/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X7_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X7_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_x7_calibration {
	float x0;
	float y0;
	float x1;
	float y1;
};

struct ctr_x7_result {
	float avg;
	float rms;
};

typedef int (*ctr_x7_api_set_power)(const struct device *dev, bool is_enabled);
typedef int (*ctr_x7_api_measure)(const struct device *dev,
				  const struct ctr_x7_calibration calibrations[2],
				  struct ctr_x7_result results[2]);

struct ctr_x7_driver_api {
	ctr_x7_api_set_power set_power;
	ctr_x7_api_measure measure;
};

static inline int ctr_x7_set_power(const struct device *dev, bool is_enabled)
{
	const struct ctr_x7_driver_api *api = (const struct ctr_x7_driver_api *)dev->api;

	return api->set_power(dev, is_enabled);
}

static inline int ctr_x7_measure(const struct device *dev,
				 const struct ctr_x7_calibration calibrations[2],
				 struct ctr_x7_result results[2])
{
	const struct ctr_x7_driver_api *api = (const struct ctr_x7_driver_api *)dev->api;

	return api->measure(dev, calibrations, results);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X7_H_ */
