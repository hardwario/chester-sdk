#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X0_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X0_H_

/* Zephyr includes */
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x0_channel {
	CTR_X0_CHANNEL_1 = 0,
	CTR_X0_CHANNEL_2 = 1,
	CTR_X0_CHANNEL_3 = 2,
	CTR_X0_CHANNEL_4 = 3,
};

enum ctr_x0_mode {
	CTR_X0_MODE_DEFAULT = 0,
	CTR_X0_MODE_NPN_INPUT = 1,
	CTR_X0_MODE_PNP_INPUT = 2,
	CTR_X0_MODE_AI_INPUT = 3,
	CTR_X0_MODE_CL_INPUT = 4,
	CTR_X0_MODE_PWR_SOURCE = 5,
};

typedef int (*ctr_x0_api_set_mode)(const struct device *dev, enum ctr_x0_channel channel,
                                   enum ctr_x0_mode mode);
typedef int (*ctr_x0_api_get_spec)(const struct device *dev, enum ctr_x0_channel channel,
                                   const struct gpio_dt_spec **spec);

struct ctr_x0_driver_api {
	ctr_x0_api_set_mode set_mode;
	ctr_x0_api_get_spec get_spec;
};

static inline int ctr_x0_set_mode(const struct device *dev, enum ctr_x0_channel channel,
                                  enum ctr_x0_mode mode)
{
	const struct ctr_x0_driver_api *api = (const struct ctr_x0_driver_api *)dev->api;

	return api->set_mode(dev, channel, mode);
}

static inline int ctr_x0_get_spec(const struct device *dev, enum ctr_x0_channel channel,
                                  const struct gpio_dt_spec **spec)
{
	const struct ctr_x0_driver_api *api = (const struct ctr_x0_driver_api *)dev->api;

	return api->get_spec(dev, channel, spec);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X0_H_ */
