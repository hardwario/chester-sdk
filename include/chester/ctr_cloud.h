/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_CLOUD_H_
#define CHESTER_INCLUDE_CTR_CLOUD_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CTR_CLOUD_TRANSFER_BUF_SIZE (16 * 1024)

struct ctr_cloud_options {
	uint64_t decoder_hash;
	uint64_t encoder_hash;
	const uint8_t *decoder_buf;
	size_t decoder_len;
	const uint8_t *encoder_buf;
	size_t encoder_len;
};

struct ctr_cloud_session {
	uint32_t id;
	uint64_t decoder_hash;
	uint64_t encoder_hash;
	uint64_t config_hash;
	int64_t timestamp;
	char device_id[36 + 1];
	char device_name[32 + 1];
};

enum ctr_cloud_event {
	CTR_CLOUD_EVENT_CONNECTED,
	CTR_CLOUD_EVENT_RECV,
};

struct ctr_cloud_event_data_recv {
	void *buf;
	size_t len;
};

union ctr_cloud_event_data {
	struct ctr_cloud_event_data_recv recv;
};

typedef void (*ctr_cloud_cb)(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param);

int ctr_cloud_init(struct ctr_cloud_options *options);
int ctr_cloud_wait_initialized(k_timeout_t timeout);
int ctr_cloud_is_initialized(bool *initialized);
int ctr_cloud_set_callback(ctr_cloud_cb user_cb, void *user_data);
int ctr_cloud_set_poll_interval(k_timeout_t interval);
int ctr_cloud_poll_immediately(void);

int ctr_cloud_send(const void *buf, size_t len);

int ctr_cloud_get_last_seen_ts(int64_t *ts);
int ctr_cloud_firmware_update(const char *firmwareId);
int ctr_cloud_recv(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CLOUD_H_ */
