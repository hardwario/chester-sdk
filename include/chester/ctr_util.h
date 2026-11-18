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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_util ctr_util
 * @{
 */

int ctr_buf2hex(const void *src, size_t src_size, char *dst, size_t dst_size, bool upper);
int ctr_hex2buf(const char *src, void *dst, size_t dst_size, bool allow_spaces);

/**
 * @brief Format a duration for humans, e.g. "45s", "36m 50s", "1d 02h 03m 04s".
 *
 * @retval 0       Success.
 * @retval -ENOSPC @p dst is too small (24 bytes always fit).
 */
int ctr_util_fmt_duration(uint32_t sec, char *dst, size_t dst_size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_UTIL_H_ */
