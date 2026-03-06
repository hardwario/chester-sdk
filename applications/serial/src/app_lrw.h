/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_LRW_H_
#define APP_LRW_H_

/* CHESTER includes */
#include <chester/ctr_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode LoRaWAN payload for first active device (legacy)
 *
 * @param buf Output buffer
 * @return int 0 on success, negative error code on failure
 */
int app_lrw_encode(struct ctr_buf *buf);

/**
 * @brief Encode LoRaWAN payload for single device
 *
 * @param buf Output buffer
 * @param device_idx Device index (0-7)
 * @return int 0 on success, negative error code on failure
 */
int app_lrw_encode_single(struct ctr_buf *buf, int device_idx);

#ifdef __cplusplus
}
#endif

#endif /* APP_LRW_H_ */
