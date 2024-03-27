#ifndef APP_CBOR_H_
#define APP_CBOR_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#include <zcbor_common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_cbor_received
{
	bool has_led_r;
	int32_t led_r;
	bool has_led_g;
	int32_t led_g;
	bool has_led_y;
	int32_t led_y;
	bool has_led_load;
	int32_t led_load;
};

int app_cbor_encode(zcbor_state_t *zs);
int app_cbor_decode(zcbor_state_t *zs, struct app_cbor_received *received);

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */
