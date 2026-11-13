/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CBOR_H_
#define APP_CBOR_H_

#include "app_send_split.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

#if FEATURE_SUBSYSTEM_LTE_V2
#include <zcbor_common.h>
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

#ifdef __cplusplus
extern "C" {
#endif

#if FEATURE_SUBSYSTEM_LTE_V2
/**
 * @brief Encode a report message
 *
 * @param zs Fresh encoder state
 * @param sel Message content
 * @retval 0 Encoded
 * @retval -ENOSPC The message does not fit the payload buffer
 * @retval other Encoding failed
 */
int app_cbor_encode(zcbor_state_t *zs, const struct app_send_select *sel);
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */