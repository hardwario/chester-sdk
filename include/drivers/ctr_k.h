#ifndef CHESTER_INCLUDE_DRIVERS_CTR_K_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_K_H_

/* Zephyr includes */
#include <device.h>
#include <drivers/gpio.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#define CTR_K_CHANNEL_SINGLE_ENDED(ch) ((ch) - 1)
#define CTR_K_CHANNEL_DIFFERENTIAL(ch) ((ch) - 1 + CTR_K_CHANNEL_1_DIFFERENTIAL)
#define CTR_K_CHANNEL_IDX(ch)                                                                      \
	((ch) >= CTR_K_CHANNEL_1_DIFFERENTIAL ? (ch) - CTR_K_CHANNEL_1_DIFFERENTIAL : (ch))
/* clang-format on */

enum ctr_k_channel {
	CTR_K_CHANNEL_1_SINGLE_ENDED = 0,
	CTR_K_CHANNEL_2_SINGLE_ENDED = 1,
	CTR_K_CHANNEL_3_SINGLE_ENDED = 2,
	CTR_K_CHANNEL_4_SINGLE_ENDED = 3,
	CTR_K_CHANNEL_1_DIFFERENTIAL = 4,
	CTR_K_CHANNEL_2_DIFFERENTIAL = 5,
	CTR_K_CHANNEL_3_DIFFERENTIAL = 6,
	CTR_K_CHANNEL_4_DIFFERENTIAL = 7,
};

struct ctr_k_result {
	float avg;
	float rms;
};

typedef int (*ctr_k_api_set_power)(const struct device *dev, enum ctr_k_channel channel,
                                   bool is_enabled);
typedef int (*ctr_k_api_measure)(const struct device *dev, const enum ctr_k_channel channels[],
                                 size_t channels_count, struct ctr_k_result results[]);

struct ctr_k_driver_api {
	ctr_k_api_set_power set_power;
	ctr_k_api_measure measure;
};

static inline int ctr_k_set_power(const struct device *dev, enum ctr_k_channel channel,
                                  bool is_enabled)
{
	const struct ctr_k_driver_api *api = (const struct ctr_k_driver_api *)dev->api;

	return api->set_power(dev, channel, is_enabled);
}

static inline int ctr_k_measure(const struct device *dev, const enum ctr_k_channel channels[],
                                size_t channels_count, struct ctr_k_result results[])
{
	const struct ctr_k_driver_api *api = (const struct ctr_k_driver_api *)dev->api;

	return api->measure(dev, channels, channels_count, results);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_K_H_ */
