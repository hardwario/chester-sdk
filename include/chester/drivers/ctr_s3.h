/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_S3_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_S3_H_

/* Zephyr includes */
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_s3_event {
	CTR_S3_EVENT_MOTION_L_DETECTED = 0,
	CTR_S3_EVENT_MOTION_R_DETECTED = 1,
};

enum ctr_s3_channel {
	CTR_S3_CHANNEL_L = 0,
	CTR_S3_CHANNEL_R = 1,
};

struct ctr_s3_param {
	int sensitivity;
	int blind_time;
	int pulse_counter;
	int window_time;
};

typedef void (*ctr_s3_user_cb)(const struct device *dev, enum ctr_s3_event event, void *user_data);
typedef int (*ctr_s3_api_set_handler)(const struct device *dev, ctr_s3_user_cb callback,
				      void *user_data);
typedef int (*ctr_s3_api_configure)(const struct device *dev, enum ctr_s3_channel channel,
				    const struct ctr_s3_param *param);

struct ctr_s3_driver_api {
	ctr_s3_api_set_handler set_handler;
	ctr_s3_api_configure configure;
};

static inline int ctr_s3_set_handler(const struct device *dev, ctr_s3_user_cb user_cb,
				     void *user_data)
{
	const struct ctr_s3_driver_api *api = (const struct ctr_s3_driver_api *)dev->api;

	return api->set_handler(dev, user_cb, user_data);
}

static inline int ctr_s3_configure(const struct device *dev, enum ctr_s3_channel channel,
				   const struct ctr_s3_param *param)
{
	const struct ctr_s3_driver_api *api = (const struct ctr_s3_driver_api *)dev->api;

	return api->configure(dev, channel, param);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_S3_H_ */
