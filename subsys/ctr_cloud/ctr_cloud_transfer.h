/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_TRANSFER_H_
#define CHESTER_SUBSYS_CTR_CLOUD_TRANSFER_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

/* CHESTER includes */
#include <chester/ctr_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_cloud_transfer_metrics {
	uint32_t uplink_count;
	uint32_t uplink_bytes;
	uint32_t uplink_fragments;
	uint32_t uplink_errors;
	int64_t uplink_last_ts;

	uint32_t downlink_count;
	uint32_t downlink_fragments;
	uint32_t downlink_bytes;
	uint32_t downlink_errors;
	int64_t downlink_last_ts;

	uint32_t poll_count;
	int64_t poll_last_ts;
};

int ctr_cloud_transfer_init(uint32_t serial_number, uint8_t token[16]);
int ctr_cloud_transfer_reset_metrics(void);
int ctr_cloud_transfer_get_metrics(struct ctr_cloud_transfer_metrics *metrics);
int ctr_cloud_transfer_uplink(struct ctr_buf *buf, bool *has_downlink);
int ctr_cloud_transfer_downlink(struct ctr_buf *buf, bool *has_downlink);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_TRANSFER_H_ */
