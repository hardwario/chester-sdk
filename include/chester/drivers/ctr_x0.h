/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X0_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X0_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_x0 ctr_x0
 * @brief Driver for CHESTER X0
 * @{
 */

/** @brief Channels */
enum ctr_x0_channel {
	CTR_X0_CHANNEL_1 = 0,
	CTR_X0_CHANNEL_2 = 1,
	CTR_X0_CHANNEL_3 = 2,
	CTR_X0_CHANNEL_4 = 3,
};

/** @brief Modes */
enum ctr_x0_mode {
	CTR_X0_MODE_DEFAULT = 0,
	CTR_X0_MODE_NPN_INPUT = 1,
	CTR_X0_MODE_PNP_INPUT = 2,
	CTR_X0_MODE_AI_INPUT = 3,
	CTR_X0_MODE_CL_INPUT = 4,
	CTR_X0_MODE_PWR_SOURCE = 5,
};

/** @private */
typedef int (*ctr_x0_api_set_mode)(const struct device *dev, enum ctr_x0_channel channel,
				   enum ctr_x0_mode mode);
/** @private */
typedef int (*ctr_x0_api_get_spec)(const struct device *dev, enum ctr_x0_channel channel,
				   const struct gpio_dt_spec **spec);

/** @private */
struct ctr_x0_driver_api {
	ctr_x0_api_set_mode set_mode;
	ctr_x0_api_get_spec get_spec;
};

/**
 * @brief Set mode
 * @param[in] dev
 * @param[in] channel Channel to modify
 * @param[in] mode
 */
static inline int ctr_x0_set_mode(const struct device *dev, enum ctr_x0_channel channel,
				  enum ctr_x0_mode mode)
{
	const struct ctr_x0_driver_api *api = (const struct ctr_x0_driver_api *)dev->api;

	return api->set_mode(dev, channel, mode);
}

/**
 * @brief Get channel's GPIO device tree spec
 * @param[in] dev
 * @param[in] channel
 * @param[out] spec
 */
static inline int ctr_x0_get_spec(const struct device *dev, enum ctr_x0_channel channel,
				  const struct gpio_dt_spec **spec)
{
	const struct ctr_x0_driver_api *api = (const struct ctr_x0_driver_api *)dev->api;

	return api->get_spec(dev, channel, spec);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X0_H_ */
