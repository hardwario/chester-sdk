/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CBOR_H_
#define APP_CBOR_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#include <zcbor_common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_cbor_received {
	bool has_version;
	int32_t version;
	bool has_led_button_0;
	int32_t led_button_0;
	bool has_led_button_1;
	int32_t led_button_1;
	bool has_led_button_2;
	int32_t led_button_2;
	bool has_led_button_3;
	int32_t led_button_3;
	bool has_led_button_4;
	int32_t led_button_4;
};

int app_cbor_encode(zcbor_state_t *zs);
int app_cbor_decode(zcbor_state_t *zs, struct app_cbor_received *received);

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */