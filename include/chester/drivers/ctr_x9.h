/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X9_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X9_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x9_output {
	CTR_X9_OUTPUT_1 = 0,
	CTR_X9_OUTPUT_2 = 1,
	CTR_X9_OUTPUT_3 = 2,
	CTR_X9_OUTPUT_4 = 3,
};

typedef int (*ctr_x9_api_set_output)(const struct device *dev, enum ctr_x9_output output,
				     bool is_on);

struct ctr_x9_driver_api {
	ctr_x9_api_set_output set_output;
};

static inline int ctr_x9_set_output(const struct device *dev, enum ctr_x9_output output, bool is_on)
{
	const struct ctr_x9_driver_api *api = (const struct ctr_x9_driver_api *)dev->api;

	return api->set_output(dev, output, is_on);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X9_H_ */
