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
	bool has_output_1_state;
	int32_t output_1_state;
	bool has_output_2_state;
	int32_t output_2_state;
	bool has_output_3_state;
	int32_t output_3_state;
	bool has_output_4_state;
	int32_t output_4_state;
};

int app_cbor_encode(zcbor_state_t *zs);
int app_cbor_decode(zcbor_state_t *zs, struct app_cbor_received *received);

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */