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

/** LoRaWAN payload format version, carried in the high nibble of byte 0.
 *
 * v1 put a single device index in the low nibble and one device per uplink.
 * v2 puts a device *count* there and packs as many devices as the MTU allows.
 */
#define APP_LRW_VERSION 2

/** Maximum LoRaWAN payload this application will transmit. */
#define APP_LRW_MTU 51

/**
 * @brief Encode as many configured devices as fit, starting at @p start_idx
 *
 * @param buf         Output buffer; must be larger than APP_LRW_MTU so a device that does not
 *                    fit can be encoded and rolled back
 * @param start_idx   First device index to consider
 * @param next_idx    Out: index to resume from; APP_CONFIG_MAX_DEVICES when all are done
 * @param packed_mask Out: BIT(idx) of every device in the message, to drop what it carried
 *                    once it is sent (NULL = not needed)
 * @return Number of devices packed (0 if none remain), or a negative errno
 */
int app_lrw_encode_multi(struct ctr_buf *buf, int start_idx, int *next_idx, uint8_t *packed_mask);

#ifdef __cplusplus
}
#endif

#endif /* APP_LRW_H_ */
