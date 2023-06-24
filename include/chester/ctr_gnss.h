/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_GNSS_H_
#define CHESTER_INCLUDE_CTR_GNSS_H_

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_gnss_event {
	CTR_GNSS_EVENT_FAILURE = -1,
	CTR_GNSS_EVENT_START_OK = 0,
	CTR_GNSS_EVENT_START_ERR = 1,
	CTR_GNSS_EVENT_STOP_OK = 2,
	CTR_GNSS_EVENT_STOP_ERR = 3,
	CTR_GNSS_EVENT_UPDATE = 4,
};

struct ctr_gnss_data_start_ok {
	int corr_id;
};

struct ctr_gnss_data_start_err {
	int corr_id;
};

struct ctr_gnss_data_stop_ok {
	int corr_id;
};

struct ctr_gnss_data_stop_err {
	int corr_id;
};

struct ctr_gnss_data_update {
	int fix_quality;
	int satellites_tracked;
	float latitude;
	float longitude;
	float altitude;
};

union ctr_gnss_event_data {
	struct ctr_gnss_data_start_ok start_ok;
	struct ctr_gnss_data_start_err start_err;
	struct ctr_gnss_data_stop_ok stop_ok;
	struct ctr_gnss_data_stop_err stop_err;
	struct ctr_gnss_data_update update;
};

typedef void (*ctr_gnss_user_cb)(enum ctr_gnss_event event, union ctr_gnss_event_data *data,
				 void *user_data);

int ctr_gnss_set_handler(ctr_gnss_user_cb user_cb, void *user_data);
int ctr_gnss_start(int *corr_id);
int ctr_gnss_stop(bool keep_bckp_domain, int *corr_id);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_GNSS_H_ */
