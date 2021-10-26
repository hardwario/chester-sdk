#ifndef CHESTER_INCLUDE_LED_H_
#define CHESTER_INCLUDE_LED_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hio_led_channel {
	HIO_LED_CHANNEL_R = 0,
	HIO_LED_CHANNEL_G = 1,
	HIO_LED_CHANNEL_Y = 2,
	HIO_LED_CHANNEL_EXT = 3,
};

int hio_led_set(enum hio_led_channel channel, bool is_on);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LED_H_ */
