/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_promag_mf7s_proto.h"

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

int drv_promag_mf7s_parse_uid(const uint8_t *hex_chars, uint32_t *uid)
{
	if (hex_chars == NULL || uid == NULL) {
		return -EINVAL;
	}

	uint32_t val = 0;

	for (int i = 0; i < 8; i++) {
		int nib = hex_nibble((char)hex_chars[i]);
		if (nib < 0) {
			return -EINVAL;
		}
		val = (val << 4) | nib;
	}

	*uid = val;
	return 0;
}

int drv_promag_mf7s_parse_card_frame(const uint8_t *buf, size_t len, uint32_t *uid,
				     size_t *consumed)
{
	if (buf == NULL || uid == NULL || consumed == NULL) {
		return -EINVAL;
	}

	/* Search for STX in buffer */
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != DRV_PROMAG_MF7S_STX) {
			continue;
		}

		/* Check if we have enough bytes for a complete frame */
		size_t remaining = len - i;
		if (remaining < DRV_PROMAG_MF7S_CARD_FRAME_LEN) {
			/* Incomplete frame - consume bytes before STX */
			*consumed = i;
			return -EAGAIN;
		}

		/* Verify frame structure: STX + 8hex + CR + LF + ETX */
		const uint8_t *frame = &buf[i];
		if (frame[9] == DRV_PROMAG_MF7S_CR && frame[10] == DRV_PROMAG_MF7S_LF &&
		    frame[11] == DRV_PROMAG_MF7S_ETX) {
			/* Verify all 8 chars are hex */
			bool valid_hex = true;
			for (int j = 1; j <= 8; j++) {
				if (hex_nibble((char)frame[j]) < 0) {
					valid_hex = false;
					break;
				}
			}

			if (valid_hex) {
				int ret = drv_promag_mf7s_parse_uid(&frame[1], uid);
				if (ret == 0) {
					*consumed = i + DRV_PROMAG_MF7S_CARD_FRAME_LEN;
					return 0;
				}
			}
		}

		/* Invalid frame at this STX - skip it */
	}

	/* No valid frame found, consume everything except a potential partial frame. */
	if (len > 0 && buf[len - 1] == DRV_PROMAG_MF7S_STX) {
		*consumed = len - 1;
	} else {
		*consumed = len;
	}
	return -EAGAIN;
}
