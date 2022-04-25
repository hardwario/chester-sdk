#ifndef CHESTER_INCLUDE_DRIVERS_CTR_X4_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_X4_H_

/* Zephyr includes */
#include <drivers/gpio.h>
#include <device.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_x4_output {
	CTR_X4_OUTPUT_1 = 0,
	CTR_X4_OUTPUT_2 = 1,
	CTR_X4_OUTPUT_3 = 2,
	CTR_X4_OUTPUT_4 = 3,
};

typedef int (*ctr_x4_api_set_output)(const struct device *dev, enum ctr_x4_output output,
                                     bool is_on);

struct ctr_x4_driver_api {
	ctr_x4_api_set_output set_output;
};

static inline int ctr_x4_set_output(const struct device *dev, enum ctr_x4_output output, bool is_on)
{
	const struct ctr_x4_driver_api *api = (const struct ctr_x4_driver_api *)dev->api;

	return api->set_output(dev, output, is_on);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_X4_H_ */
