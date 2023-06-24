/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LTE_H_
#define CHESTER_INCLUDE_CTR_LTE_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_event {
	CTR_LTE_EVENT_FAILURE = -1,
	CTR_LTE_EVENT_START_OK = 0,
	CTR_LTE_EVENT_START_ERR = 1,
	CTR_LTE_EVENT_ATTACH_OK = 2,
	CTR_LTE_EVENT_ATTACH_ERR = 3,
	CTR_LTE_EVENT_DETACH_OK = 4,
	CTR_LTE_EVENT_DETACH_ERR = 5,
	CTR_LTE_EVENT_EVAL_OK = 6,
	CTR_LTE_EVENT_EVAL_ERR = 7,
	CTR_LTE_EVENT_SEND_OK = 8,
	CTR_LTE_EVENT_SEND_ERR = 9,
};

struct ctr_lte_eval {
	int eest;
	int ecl;
	int rsrp;
	int rsrq;
	int snr;
	int plmn;
	int cid;
	int band;
	int earfcn;
};

struct ctr_lte_data_start_ok {
	int corr_id;
};

struct ctr_lte_data_start_err {
	int corr_id;
};

struct ctr_lte_data_attach_ok {
	int corr_id;
};

struct ctr_lte_data_attach_err {
	int corr_id;
};

struct ctr_lte_data_detach_ok {
	int corr_id;
};

struct ctr_lte_data_detach_err {
	int corr_id;
};

struct ctr_lte_data_send_ok {
	int corr_id;
};

struct ctr_lte_data_send_err {
	int corr_id;
};

struct ctr_lte_data_eval_ok {
	int corr_id;
	struct ctr_lte_eval eval;
};

struct ctr_lte_data_eval_err {
	int corr_id;
};

union ctr_lte_event_data {
	struct ctr_lte_data_start_ok start_ok;
	struct ctr_lte_data_start_err start_err;
	struct ctr_lte_data_attach_ok attach_ok;
	struct ctr_lte_data_attach_err attach_err;
	struct ctr_lte_data_detach_ok detach_ok;
	struct ctr_lte_data_detach_err detach_err;
	struct ctr_lte_data_eval_ok eval_ok;
	struct ctr_lte_data_eval_err eval_err;
	struct ctr_lte_data_send_ok send_ok;
	struct ctr_lte_data_send_err send_err;
};

typedef void (*ctr_lte_event_cb)(enum ctr_lte_event event, union ctr_lte_event_data *data,
				 void *param);

struct ctr_lte_send_opts {
	int64_t ttl;
	uint8_t addr[4];
	int port;
};

#define CTR_LTE_SEND_OPTS_DEFAULTS                                                                 \
	{                                                                                          \
		.ttl = 0, .addr = {0, 0, 0, 0}, .port = -1,                                        \
	}

int ctr_lte_get_imsi(uint64_t *imsi);
int ctr_lte_set_event_cb(ctr_lte_event_cb cb, void *user_data);
int ctr_lte_get_imei(uint64_t *imei);
int ctr_lte_get_imsi(uint64_t *imsi);
int ctr_lte_is_attached(bool *attached);
int ctr_lte_start(int *corr_id);
int ctr_lte_attach(int *corr_id);
int ctr_lte_detach(int *corr_id);
int ctr_lte_eval(int *corr_id);
int ctr_lte_send(const struct ctr_lte_send_opts *opts, const void *buf, size_t len, int *corr_id);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LTE_H_ */
