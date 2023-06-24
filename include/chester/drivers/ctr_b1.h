/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_B1_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_B1_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_b1_output {
	CTR_B1_OUTPUT_WM_RESET = 0,
	CTR_B1_OUTPUT_WM_ON = 1,
	CTR_B1_OUTPUT_ANT_1 = 2,
	CTR_B1_OUTPUT_ANT_2 = 3,
};

typedef int (*ctr_b1_api_set_output)(const struct device *dev, enum ctr_b1_output output,
				     int value);

struct ctr_b1_driver_api {
	ctr_b1_api_set_output set_output;
};

static inline int ctr_b1_set_output(const struct device *dev, enum ctr_b1_output output, int value)
{
	const struct ctr_b1_driver_api *api = (const struct ctr_b1_driver_api *)dev->api;

	return api->set_output(dev, output, value);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_B1_H_ */
