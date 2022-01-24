#ifndef CHESTER_INCLUDE_DRIVERS_BATT_H_
#define CHESTER_INCLUDE_DRIVERS_BATT_H_

#include <device.h>

#define CTR_BATT_REST_TIMEOUT_DEFAULT_MS 1000
#define CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS 9000

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ctr_batt_api_get_rest_voltage_mv)(const struct device *dev, int *rest_mv,
                                                k_timeout_t timeout);
typedef int (*ctr_batt_api_get_load_voltage_mv)(const struct device *dev, int *load_mv,
                                                k_timeout_t timeout);
typedef void (*ctr_batt_api_get_load_current_ma)(const struct device *dev, int *current_ma,
                                                 int load_mv);

struct ctr_batt_driver_api {
	ctr_batt_api_get_rest_voltage_mv get_rest_voltage_mv;
	ctr_batt_api_get_load_voltage_mv get_load_voltage_mv;
	ctr_batt_api_get_load_current_ma get_load_current_ma;
};

static inline int ctr_batt_get_rest_voltage_mv(const struct device *dev, int *rest_mv,
                                               k_timeout_t timeout)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->get_rest_voltage_mv(dev, rest_mv, timeout);
}

static inline int ctr_batt_get_load_voltage_mv(const struct device *dev, int *load_mv,
                                               k_timeout_t timeout)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	return api->get_load_voltage_mv(dev, load_mv, timeout);
}

static inline void ctr_batt_get_load_current_ma(const struct device *dev, int *current_ma,
                                                int load_mv)
{
	const struct ctr_batt_driver_api *api = (const struct ctr_batt_driver_api *)dev->api;

	api->get_load_current_ma(dev, current_ma, load_mv);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_BATT_H_ */
