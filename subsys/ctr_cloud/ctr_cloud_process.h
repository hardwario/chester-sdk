/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_PROCESS_H_
#define CHESTER_SUBSYS_CTR_CLOUD_PROCESS_H_

#include "ctr_cloud_msg.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_cloud_process_dlconfig(struct ctr_cloud_msg_dlconfig *msg);
int ctr_cloud_process_dlshell(struct ctr_cloud_msg_dlshell *msg, struct ctr_buf *buf);
int ctr_cloud_process_dlfirmware(struct ctr_cloud_msg_dlfirmware *msg, struct ctr_buf *buf);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_PROCESS_H_ */
