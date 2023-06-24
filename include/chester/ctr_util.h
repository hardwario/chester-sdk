/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_UTIL_H_
#define CHESTER_INCLUDE_CTR_UTIL_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_buf2hex(const void *src, size_t src_size, char *dst, size_t dst_size, bool upper);
int ctr_hex2buf(const char *src, void *dst, size_t dst_size, bool allow_spaces);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_UTIL_H_ */
