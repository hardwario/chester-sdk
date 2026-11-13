/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_promag_mf7s_proto.h
 * @brief Pure Promag MF7S card-frame parser (no Zephyr / app dependencies)
 *
 * The card frame parsing is split out from the driver so it can be unit-tested on the
 * host without a UART. The listener thread, PM and LED handling stay in the driver.
 *
 * Card frame: STX + 8 ASCII hex (UID) + CR + LF + ETX = 12 bytes.
 */

#ifndef DRV_PROMAG_MF7S_PROTO_H_
#define DRV_PROMAG_MF7S_PROTO_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_PROMAG_MF7S_STX 0x02
#define DRV_PROMAG_MF7S_ETX 0x03
#define DRV_PROMAG_MF7S_CR  0x0D
#define DRV_PROMAG_MF7S_LF  0x0A

/* STX + 8 hex + CR + LF + ETX */
#define DRV_PROMAG_MF7S_CARD_FRAME_LEN 12

/**
 * @brief Parse 8 ASCII hex characters into a big-endian UID
 *
 * @param hex_chars 8 characters, MSB nibble first
 * @param uid       Output UID
 * @retval 0 on success
 * @retval -EINVAL if @p hex_chars or @p uid is NULL, or a character is not hex
 */
int drv_promag_mf7s_parse_uid(const uint8_t *hex_chars, uint32_t *uid);

/**
 * @brief Find and decode the first complete card frame in @p buf
 *
 * Scans for STX, checks the STX + 8 hex + CR + LF + ETX structure, and decodes the UID.
 * @p consumed reports how many leading bytes the caller may drop from its accumulation
 * buffer, so a partial trailing frame is preserved for the next read.
 *
 * @param buf      Accumulated received bytes
 * @param len      Number of bytes in @p buf
 * @param uid      Output UID (only written on success)
 * @param consumed Output: leading bytes to drop from @p buf
 * @retval 0       a complete valid frame was decoded
 * @retval -EAGAIN no complete valid frame yet (consumed set to what can be dropped)
 * @retval -EINVAL @p buf, @p uid or @p consumed is NULL
 */
int drv_promag_mf7s_parse_card_frame(const uint8_t *buf, size_t len, uint32_t *uid,
				     size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* DRV_PROMAG_MF7S_PROTO_H_ */
