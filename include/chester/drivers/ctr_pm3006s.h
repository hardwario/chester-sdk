/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_PM3006S_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_PM3006S_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard include */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_pm3006s ctr_pm3006s
 * @{
 */


/* Define possible sensor statuses */
enum ctr_pm3006s_sensor_status {
	/* 1: Sensor is closing */
	CTR_PM3006S_STATUS_CLOSING = 1,

	/* 2: Sensor is measuring */
	CTR_PM3006S_STATUS_MEASURING = 2,

	/* 7: Alarming */
	CTR_PM3006S_STATUS_ALARMING = 7,

	/* 0x80: Finish measurement */
	CTR_PM3006S_STATUS_FINISH_MEASUREMENT = 0x80
};

/* Define the struct for the PM3006 sensor measurement */
struct ctr_pm3006s_measurement {
	uint16_t sensor_status;          /* Sensor status */
	uint16_t tsp;                    /* Total Suspended Particles (TSP) */
	uint16_t pm1;                    /* Particulate matter (PM) 1 */
	uint16_t pm2_5;                  /* Particulate matter (PM) 2.5 */
	uint16_t pm10;                   /* Particulate matter (PM) 10 */
	uint32_t q0_3um;                 /* 0.3 um particles */
	uint32_t q0_5um;                 /* 0.5 um particles */
	uint32_t q1_0um;                 /* 1.0 um particles */
	uint32_t q2_5um;                 /* 2.5 um particles */
	uint32_t q5_0um;                 /* 5.0 um particles */
	uint32_t q10um;                  /* 10. 0um particles */
};

/* Define the API for the PM3006 sensor */
typedef int (*ctr_pm3006s_api_read_measurement)(const struct device *dev,
						   struct ctr_pm3006s_measurement *measurement);
typedef int (*ctr_pm3006s_api_open_measurement)(const struct device *dev);

typedef int (*ctr_pm3006s_api_close_measurement)(const struct device *dev);


struct ctr_pm3006s_driver_api {
    ctr_pm3006s_api_read_measurement read_measurement;
    ctr_pm3006s_api_open_measurement open_measurement;
    ctr_pm3006s_api_close_measurement close_measurement;
};


static inline int ctr_pm3006s_open_measurement(const struct device *dev)
{
	const struct ctr_pm3006s_driver_api *api =
		(const struct ctr_pm3006s_driver_api *)dev->api;

	return api->open_measurement(dev);
}

static inline int ctr_pm3006s_close_measurement(const struct device *dev)
{
	const struct ctr_pm3006s_driver_api *api =
		(const struct ctr_pm3006s_driver_api *)dev->api;

	return api->close_measurement(dev);
}

static inline int ctr_pm3006s_read_measurement(const struct device *dev,
					       struct ctr_pm3006s_measurement *measurement)
{
	const struct ctr_pm3006s_driver_api *api =
		(const struct ctr_pm3006s_driver_api *)dev->api;

	return api->read_measurement(dev, measurement);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_PM3006S_H_ */
