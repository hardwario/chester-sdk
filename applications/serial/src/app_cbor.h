/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CBOR_H_
#define APP_CBOR_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#if FEATURE_SUBSYSTEM_LTE_V2
#include <zcbor_common.h>
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

#ifdef __cplusplus
extern "C" {
#endif

#if FEATURE_SUBSYSTEM_LTE_V2
int app_cbor_encode(zcbor_state_t *zs);
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */