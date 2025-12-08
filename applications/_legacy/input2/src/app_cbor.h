/*
 * Copyright (c) 2023 HARDWARIO a.s.
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

int app_cbor_encode(zcbor_state_t *zs);

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */
