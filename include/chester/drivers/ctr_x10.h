/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X10_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X10_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x10_event {
	CTR_X10_EVENT_LINE_CONNECTED = 0,
	CTR_X10_EVENT_LINE_DISCONNECTED = 1,
};

typedef void (*ctr_x10_user_cb)(const struct device *dev, enum ctr_x10_event event,
				void *user_data);
typedef int (*ctr_x10_api_set_handler)(const struct device *dev, ctr_x10_user_cb callback,
				       void *user_data);
typedef int (*ctr_x10_api_get_line_voltage)(const struct device *dev, int *line_voltage_mv);
typedef int (*ctr_x10_api_get_line_present)(const struct device *dev, bool *is_line_present);
typedef int (*ctr_x10_api_get_battery_voltage)(const struct device *dev, int *battery_voltage_mv);

struct ctr_x10_driver_api {
	ctr_x10_api_set_handler set_handler;
	ctr_x10_api_get_line_voltage get_line_voltage;
	ctr_x10_api_get_line_present get_line_present;
	ctr_x10_api_get_battery_voltage get_battery_voltage;
};

static inline int ctr_x10_set_handler(const struct device *dev, ctr_x10_user_cb user_cb,
				      void *user_data)
{
	const struct ctr_x10_driver_api *api = (const struct ctr_x10_driver_api *)dev->api;

	return api->set_handler(dev, user_cb, user_data);
}

static inline int ctr_x10_get_line_voltage(const struct device *dev, int *line_voltage_mv)
{
	const struct ctr_x10_driver_api *api = (const struct ctr_x10_driver_api *)dev->api;

	return api->get_line_voltage(dev, line_voltage_mv);
}

static inline int ctr_x10_get_line_present(const struct device *dev, bool *is_line_present)
{
	const struct ctr_x10_driver_api *api = (const struct ctr_x10_driver_api *)dev->api;

	return api->get_line_present(dev, is_line_present);
}

static inline int ctr_x10_get_battery_voltage(const struct device *dev, int *battery_voltage_mv)
{
	const struct ctr_x10_driver_api *api = (const struct ctr_x10_driver_api *)dev->api;

	return api->get_battery_voltage(dev, battery_voltage_mv);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X10_H_ */
