/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_PEOPLE_COUNTER_H_
#define CHESTER_INCLUDE_DRIVERS_PEOPLE_COUNTER_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard include */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct people_counter_measurement {
	uint16_t motion_counter;
	uint16_t pass_counter_adult;
	uint16_t pass_counter_child;
	uint16_t stay_counter_adult;
	uint16_t stay_counter_child;
	uint16_t total_time_adult;
	uint16_t total_time_child;
	uint32_t consumed_energy;
};

typedef int (*people_counter_api_read_measurement)(const struct device *dev,
						   struct people_counter_measurement *measurement);
typedef int (*people_counter_api_get_power_off_delay)(const struct device *dev, int *value);
typedef int (*people_counter_api_get_stay_timeout)(const struct device *dev, int *value);
typedef int (*people_counter_api_get_adult_border)(const struct device *dev, int *value);
typedef int (*people_counter_api_set_power_off_delay)(const struct device *dev, int value);
typedef int (*people_counter_api_set_stay_timeout)(const struct device *dev, int value);
typedef int (*people_counter_api_set_adult_border)(const struct device *dev, int value);

struct people_counter_driver_api {
	people_counter_api_read_measurement read_measurement;
	people_counter_api_get_power_off_delay get_power_off_delay;
	people_counter_api_get_stay_timeout get_stay_timeout;
	people_counter_api_get_adult_border get_adult_border;
	people_counter_api_set_power_off_delay set_power_off_delay;
	people_counter_api_set_stay_timeout set_stay_timeout;
	people_counter_api_set_adult_border set_adult_border;
};

static inline int people_counter_read_measurement(const struct device *dev,
						  struct people_counter_measurement *measurement)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->read_measurement(dev, measurement);
}

static inline int people_counter_get_power_off_delay(const struct device *dev, int *value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->get_power_off_delay(dev, value);
}

static inline int people_counter_get_stay_timeout(const struct device *dev, int *value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->get_stay_timeout(dev, value);
}

static inline int people_counter_get_adult_border(const struct device *dev, int *value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->get_adult_border(dev, value);
}

static inline int people_counter_set_power_off_delay(const struct device *dev, int value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->set_power_off_delay(dev, value);
}

static inline int people_counter_set_stay_timeout(const struct device *dev, int value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->set_stay_timeout(dev, value);
}

static inline int people_counter_set_adult_border(const struct device *dev, int value)
{
	const struct people_counter_driver_api *api =
		(const struct people_counter_driver_api *)dev->api;

	return api->set_adult_border(dev, value);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_PEOPLE_COUNTER_H_ */
