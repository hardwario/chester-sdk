/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdint.h>

int ctr_buf2hex(const void *src, size_t src_size, char *dst, size_t dst_size, bool upper)
{
	if (src_size * 2 + 1 != dst_size) {
		return -EINVAL;
	}

	char x = upper ? 'A' : 'a';

	size_t n;
	const uint8_t *p;
	char *d = dst;

	for (n = 0, p = (uint8_t *)src; n < src_size; n++, p++) {
		*d++ = (*p >> 4) >= 10 ? (*p >> 4) - 10 + x : (*p >> 4) + '0';
		*d++ = (*p & 15) >= 10 ? (*p & 15) - 10 + x : (*p & 15) + '0';
	}

	*d = '\0';

	return n * 2;
}

int ctr_hex2buf(const char *src, void *dst, size_t dst_size, bool allow_spaces)
{
	size_t n;
	const char *p;

	for (n = 0, p = src; *p != '\0'; p++) {
		if (allow_spaces && *p == ' ') {
			continue;
		}

		if ((*p < '0' || *p > '9') && (*p < 'a' || *p > 'f') && (*p < 'A' || *p > 'F')) {
			return -EINVAL;
		}

		n++;
	}

	if (n != dst_size * 2) {
		return -EINVAL;
	}

	uint8_t *d = (uint8_t *)dst;

	for (n = 0, p = src; *p != '\0'; p++) {
		if (allow_spaces && *p == ' ') {
			continue;
		}

		uint8_t nibble;

		if (*p >= 'a' && *p <= 'f') {
			nibble = (uint8_t)(*p - 'a') + 10;
		} else if (*p >= 'A' && *p <= 'F') {
			nibble = (uint8_t)(*p - 'A') + 10;
		} else {
			nibble = (uint8_t)(*p - '0');
		}

		if (n++ % 2 == 0) {
			*d = nibble << 4;
		} else {
			*d++ |= nibble;
		}
	}

	return n / 2;
}
