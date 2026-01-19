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
#include <stdint.h>

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_cloud_transfer_event {
	CTR_CLOUD_TRANSFER_EVENT_UPLINK_OK,
	CTR_CLOUD_TRANSFER_EVENT_UPLINK_ERROR,
	CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_OK,
	CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_ERROR,
	CTR_CLOUD_TRANSFER_EVENT_POLL,
};

struct ctr_cloud_transfer_event_data {
	uint32_t fragments;
	uint32_t bytes;
};

typedef void (*ctr_cloud_transfer_cb)(enum ctr_cloud_transfer_event event,
				      const struct ctr_cloud_transfer_event_data *data);

int ctr_cloud_transfer_init(uint32_t serial_number, uint8_t token[16],
			    ctr_cloud_transfer_cb cb);
int ctr_cloud_transfer_wait_for_ready(k_timeout_t timeout);
int ctr_cloud_transfer_uplink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout);
int ctr_cloud_transfer_downlink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_TRANSFER_H_ */
