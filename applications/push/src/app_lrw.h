/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_LRW_H_
#define APP_LRW_H_

#include <chester/ctr_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_lrw_encode(struct ctr_buf *buf);

#ifdef __cplusplus
}
#endif

#endif /* APP_LRW_H_ */
