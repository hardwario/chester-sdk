#ifndef HIO_NET_LRW_H
#define HIO_NET_LRW_H

// Standard includes
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hio_net_lrw_event {
	HIO_NET_LRW_EVENT_FAILURE = -1,
	HIO_NET_LRW_EVENT_START_OK = 0,
	HIO_NET_LRW_EVENT_START_ERR = 1,
	HIO_NET_LRW_EVENT_JOIN_OK = 2,
	HIO_NET_LRW_EVENT_JOIN_ERR = 3,
	HIO_NET_LRW_EVENT_SEND_OK = 4,
	HIO_NET_LRW_EVENT_SEND_ERR = 5,
};

struct hio_net_lrw_data_start_ok {
	int corr_id;
};

struct hio_net_lrw_data_start_err {
	int corr_id;
};

struct hio_net_lrw_data_join_ok {
	int corr_id;
};

struct hio_net_lrw_data_join_err {
	int corr_id;
};

struct hio_net_lrw_data_send_ok {
	int corr_id;
};

struct hio_net_lrw_data_send_err {
	int corr_id;
};

union hio_net_lrw_event_data {
	struct hio_net_lrw_data_start_ok start_ok;
	struct hio_net_lrw_data_start_err start_err;
	struct hio_net_lrw_data_join_ok join_ok;
	struct hio_net_lrw_data_join_err join_err;
	struct hio_net_lrw_data_send_ok send_ok;
	struct hio_net_lrw_data_send_err send_err;
};

typedef void (*hio_net_lrw_event_cb)(enum hio_net_lrw_event event,
                                     union hio_net_lrw_event_data *data, void *param);

struct hio_net_lrw_send_opts {
	int64_t ttl;
	bool confirmed;
	int port;
};

#define HIO_NET_LRW_SEND_OPTS_DEFAULTS                                                             \
	{                                                                                          \
		.ttl = 0, .confirmed = false, .port = -1,                                          \
	}

int hio_net_lrw_init(hio_net_lrw_event_cb callback, void *param);
int hio_net_lrw_start(int *corr_id);
int hio_net_lrw_join(int *corr_id);
int hio_net_lrw_send(const struct hio_net_lrw_send_opts *opts, const void *buf, size_t len,
                     int *corr_id);

#ifdef __cplusplus
}
#endif

#endif
