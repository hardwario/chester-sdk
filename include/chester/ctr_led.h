#ifndef CHESTER_INCLUDE_CTR_LED_H_
#define CHESTER_INCLUDE_CTR_LED_H_

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_led_channel {
	CTR_LED_CHANNEL_R = 0,
	CTR_LED_CHANNEL_G = 1,
	CTR_LED_CHANNEL_Y = 2,
	CTR_LED_CHANNEL_EXT = 3,
	CTR_LED_CHANNEL_LOAD = 4,
};

int ctr_led_set(enum ctr_led_channel channel, bool is_on);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LED_H_ */
