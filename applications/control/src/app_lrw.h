/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_LRW_H_
#define APP_LRW_H_

#include <chester/ctr_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Encodes the report into `buf`. When it does not fit one 51-byte frame, channels 5-8
 * (the X0-B block) go into `buf_b` as a second uplink; otherwise `buf_b` is left empty. */
int app_lrw_encode(struct ctr_buf *buf, struct ctr_buf *buf_b);

#ifdef __cplusplus
}
#endif

#endif /* APP_LRW_H_ */
