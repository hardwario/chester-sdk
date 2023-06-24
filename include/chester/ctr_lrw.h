/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LRW_H_
#define CHESTER_INCLUDE_CTR_LRW_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lrw_event {
	CTR_LRW_EVENT_FAILURE = -1,
	CTR_LRW_EVENT_START_OK = 0,
	CTR_LRW_EVENT_START_ERR = 1,
	CTR_LRW_EVENT_JOIN_OK = 2,
	CTR_LRW_EVENT_JOIN_ERR = 3,
	CTR_LRW_EVENT_SEND_OK = 4,
	CTR_LRW_EVENT_SEND_ERR = 5,
};

struct ctr_lrw_data_start_ok {
	int corr_id;
};

struct ctr_lrw_data_start_err {
	int corr_id;
};

struct ctr_lrw_data_join_ok {
	int corr_id;
};

struct ctr_lrw_data_join_err {
	int corr_id;
};

struct ctr_lrw_data_send_ok {
	int corr_id;
};

struct ctr_lrw_data_send_err {
	int corr_id;
};

union ctr_lrw_event_data {
	struct ctr_lrw_data_start_ok start_ok;
	struct ctr_lrw_data_start_err start_err;
	struct ctr_lrw_data_join_ok join_ok;
	struct ctr_lrw_data_join_err join_err;
	struct ctr_lrw_data_send_ok send_ok;
	struct ctr_lrw_data_send_err send_err;
};

typedef void (*ctr_lrw_event_cb)(enum ctr_lrw_event event, union ctr_lrw_event_data *data,
				 void *param);

struct ctr_lrw_send_opts {
	int64_t ttl;
	bool confirmed;
	int port;
};

#define CTR_LRW_SEND_OPTS_DEFAULTS                                                                 \
	{                                                                                          \
		.ttl = 0, .confirmed = false, .port = -1,                                          \
	}

int ctr_lrw_init(ctr_lrw_event_cb callback, void *param);
int ctr_lrw_start(int *corr_id);
int ctr_lrw_join(int *corr_id);
int ctr_lrw_send(const struct ctr_lrw_send_opts *opts, const void *buf, size_t len, int *corr_id);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LRW_H_ */
