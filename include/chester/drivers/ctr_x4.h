/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X4_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X4_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x4_event {
	CTR_X4_EVENT_LINE_CONNECTED = 0,
	CTR_X4_EVENT_LINE_DISCONNECTED = 1,
};

enum ctr_x4_output {
	CTR_X4_OUTPUT_1 = 0,
	CTR_X4_OUTPUT_2 = 1,
	CTR_X4_OUTPUT_3 = 2,
	CTR_X4_OUTPUT_4 = 3,
};

typedef void (*ctr_x4_user_cb)(const struct device *dev, enum ctr_x4_event event, void *user_data);
typedef int (*ctr_x4_api_set_handler)(const struct device *dev, ctr_x4_user_cb callback,
				      void *user_data);
typedef int (*ctr_x4_api_set_output)(const struct device *dev, enum ctr_x4_output output,
				     bool is_on);
typedef int (*ctr_x4_api_get_line_voltage)(const struct device *dev, int *line_voltage_mv);
typedef int (*ctr_x4_api_get_line_present)(const struct device *dev, bool *is_line_present);

struct ctr_x4_driver_api {
	ctr_x4_api_set_handler set_handler;
	ctr_x4_api_set_output set_output;
	ctr_x4_api_get_line_voltage get_line_voltage;
	ctr_x4_api_get_line_present get_line_present;
};

static inline int ctr_x4_set_handler(const struct device *dev, ctr_x4_user_cb user_cb,
				     void *user_data)
{
	const struct ctr_x4_driver_api *api = (const struct ctr_x4_driver_api *)dev->api;

	return api->set_handler(dev, user_cb, user_data);
}

static inline int ctr_x4_set_output(const struct device *dev, enum ctr_x4_output output, bool is_on)
{
	const struct ctr_x4_driver_api *api = (const struct ctr_x4_driver_api *)dev->api;

	return api->set_output(dev, output, is_on);
}

static inline int ctr_x4_get_line_voltage(const struct device *dev, int *line_voltage_mv)
{
	const struct ctr_x4_driver_api *api = (const struct ctr_x4_driver_api *)dev->api;

	return api->get_line_voltage(dev, line_voltage_mv);
}

static inline int ctr_x4_get_line_present(const struct device *dev, bool *is_line_present)
{
	const struct ctr_x4_driver_api *api = (const struct ctr_x4_driver_api *)dev->api;

	return api->get_line_present(dev, is_line_present);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X4_H_ */
