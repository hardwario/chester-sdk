/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_K1_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_K1_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#define CTR_K1_CHANNEL_SINGLE_ENDED(ch) ((ch) - 1)
#define CTR_K1_CHANNEL_DIFFERENTIAL(ch) ((ch) - 1 + CTR_K1_CHANNEL_1_DIFFERENTIAL)
#define CTR_K1_CHANNEL_IDX(ch)                                                                      \
	((ch) >= CTR_K1_CHANNEL_1_DIFFERENTIAL ? (ch) - CTR_K1_CHANNEL_1_DIFFERENTIAL : (ch))
/* clang-format on */

enum ctr_k1_channel {
	CTR_K1_CHANNEL_1_SINGLE_ENDED = 0,
	CTR_K1_CHANNEL_2_SINGLE_ENDED = 1,
	CTR_K1_CHANNEL_3_SINGLE_ENDED = 2,
	CTR_K1_CHANNEL_4_SINGLE_ENDED = 3,
	CTR_K1_CHANNEL_1_DIFFERENTIAL = 4,
	CTR_K1_CHANNEL_2_DIFFERENTIAL = 5,
	CTR_K1_CHANNEL_3_DIFFERENTIAL = 6,
	CTR_K1_CHANNEL_4_DIFFERENTIAL = 7,
};

struct ctr_k1_calibration {
	float x0;
	float y0;
	float x1;
	float y1;
};

struct ctr_k1_result {
	float avg;
	float rms;
};

typedef int (*ctr_k1_api_set_power)(const struct device *dev, enum ctr_k1_channel channel,
				    bool is_enabled);
typedef int (*ctr_k1_api_measure)(const struct device *dev, const enum ctr_k1_channel channels[],
				  size_t channels_count,
				  const struct ctr_k1_calibration calibrations[],
				  struct ctr_k1_result results[]);

struct ctr_k1_driver_api {
	ctr_k1_api_set_power set_power;
	ctr_k1_api_measure measure;
};

static inline int ctr_k1_set_power(const struct device *dev, enum ctr_k1_channel channel,
				   bool is_enabled)
{
	const struct ctr_k1_driver_api *api = (const struct ctr_k1_driver_api *)dev->api;

	return api->set_power(dev, channel, is_enabled);
}

static inline int ctr_k1_measure(const struct device *dev, const enum ctr_k1_channel channels[],
				 size_t channels_count,
				 const struct ctr_k1_calibration calibrations[],
				 struct ctr_k1_result results[])
{
	const struct ctr_k1_driver_api *api = (const struct ctr_k1_driver_api *)dev->api;

	return api->measure(dev, channels, channels_count, calibrations, results);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_K1_H_ */
