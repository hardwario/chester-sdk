/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_SPOOL_H_
#define CHESTER_SUBSYS_CTR_CLOUD_SPOOL_H_

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Backend-agnostic spool storage API. The backend is selected with the
 * CTR_CLOUD_SPOOL_BACKEND Kconfig choice; exactly one implementation is
 * compiled in.
 */

/* Backend-specific message identifier, opaque to the caller (LittleFS
 * backend: UTC timestamp in milliseconds; a future flash backend may use
 * e.g. a record address). */
typedef uint64_t ctr_cloud_spool_id;

/* Store one message frame (incl. the protocol header). Drops the oldest
 * message(s) when the spool is full (limit given by the spool-size config
 * parameter). The identifier of the stored message is copied to `id` (pass
 * NULL when not needed). */
int ctr_cloud_spool_save(const void *buf, size_t len, ctr_cloud_spool_id *id);

/* Find the oldest stored message. Returns 0 when found, 1 when the spool
 * is empty, negative on error. */
int ctr_cloud_spool_peek(ctr_cloud_spool_id *id);

/* Append the message content at the buffer's current position (the buffer
 * is NOT reset - the caller may pre-fill it, e.g. with a frame header). */
int ctr_cloud_spool_load(ctr_cloud_spool_id id, struct ctr_buf *buf);
int ctr_cloud_spool_delete(ctr_cloud_spool_id id);
int ctr_cloud_spool_count(int *count);
int ctr_cloud_spool_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_SPOOL_H_ */
