/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_piketronic_rppu_proto.h
 * @brief Pure Piketronic RPP-U frame codec (no Zephyr / app dependencies)
 *
 * The wire protocol is split out from the driver so it can be unit-tested on the host
 * without a UART or the SC16IS740 bridge. Everything here is a pure function of its
 * input bytes; the transport (wake/park/PM, send/receive) stays in the driver.
 *
 * Frame format:  '@' | nB_H | nB_L | Cmd | data... | CSUM
 *   nB   = number of bytes after nB excluding CSUM (i.e. Cmd + data), MSB first
 *   CSUM = low byte of the 8-bit sum of every byte except the leading '@'
 */

#ifndef DRV_PIKETRONIC_RPPU_PROTO_H_
#define DRV_PIKETRONIC_RPPU_PROTO_H_

#include "drv_piketronic_rppu.h"

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_PIKETRONIC_RPPU_FRAME_START 0x40 /* '@' */

/* A reply is 4 header/cmd bytes (@ nB_H nB_L Cmd) + payload + 1 CSUM byte. */
#define DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN 61
#define DRV_PIKETRONIC_RPPU_DATA_FRAME_LEN   (4 + DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN + 1) /* 66 */
#define DRV_PIKETRONIC_RPPU_SERIAL_ASCII_LEN 10
#define DRV_PIKETRONIC_RPPU_SERIAL_FRAME_LEN (4 + DRV_PIKETRONIC_RPPU_SERIAL_ASCII_LEN + 1) /* 15 */

/**
 * @brief Low byte of the 8-bit sum of @p len bytes starting at @p after_start
 *
 * @p after_start points at the first byte after the leading '@', i.e. nB_H. The
 * checksum covers everything except that '@'.
 *
 * @param after_start First byte covered (nB_H)
 * @param len         Number of bytes to sum
 * @return Checksum byte
 */
uint8_t drv_piketronic_rppu_checksum(const uint8_t *after_start, size_t len);

/**
 * @brief Validate a received reply frame of @p len bytes
 *
 * Checks the leading '@', that the nB field matches @p len, and the trailing checksum.
 * Works for any reply length (DATA, serial-number, ...), since nB is derived from @p len.
 *
 * @param rx  Received bytes
 * @param len Number of bytes received (the full frame including CSUM)
 * @retval 0 on a well-formed frame
 * @retval -EINVAL  if @p rx is NULL or @p len is too short to hold a frame
 * @retval -EBADMSG on bad start byte, wrong nB, or checksum mismatch
 */
int drv_piketronic_rppu_verify_frame(const uint8_t *rx, size_t len);

/**
 * @brief Decode a DATA ('D') reply into @p out
 *
 * Verifies the frame first (see drv_piketronic_rppu_verify_frame), then decodes the
 * 61-byte payload. Only the decoded fields are written; the stateful bookkeeping
 * (valid flag, read_ok / read_fail counters) is left to the caller.
 *
 * @param frame Received bytes
 * @param len   Number of bytes received; must equal DRV_PIKETRONIC_RPPU_DATA_FRAME_LEN
 * @param out   Destination
 * @retval 0 on success
 * @retval -EINVAL  if @p frame or @p out is NULL
 * @retval -EBADMSG on a malformed frame or a length other than a DATA frame
 */
int drv_piketronic_rppu_decode_data(const uint8_t *frame, size_t len,
				    struct drv_piketronic_rppu_data *out);

#ifdef __cplusplus
}
#endif

#endif /* DRV_PIKETRONIC_RPPU_PROTO_H_ */
