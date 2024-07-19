#ifndef CHESTER_INCLUDE_CTR_LRW_V2_H_
#define CHESTER_INCLUDE_CTR_LRW_V2_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lrw_v2_event {
	CTR_LRW_V2_EVENT_FAILURE = -1,
	CTR_LRW_V2_EVENT_START_OK = 0,
	CTR_LRW_V2_EVENT_START_ERR = 1,
	CTR_LRW_V2_EVENT_JOIN_OK = 2,
	CTR_LRW_V2_EVENT_JOIN_ERR = 3,
	CTR_LRW_V2_EVENT_SEND_OK = 4,
	CTR_LRW_V2_EVENT_SEND_ERR = 5,
	CTR_LRW_V2_EVENT_RECV = 6,
};

typedef void (*ctr_lrw_v2_recv_cb)(enum ctr_lrw_v2_event event, const char *line, void *param);

struct send_msgq_data {
	int64_t ttl;
	bool confirmed;
	int port;
	void *buf;
	size_t len;
};

struct ctr_lrw_send_opts {
	int64_t ttl;
	bool confirmed;
	int port;
};

#define CTR_LRW_SEND_OPTS_DEFAULTS                                                                 \
	{                                                                                          \
		.ttl = 0, .confirmed = false, .port = -1,                                          \
	}

int ctr_lrw_v2_init(ctr_lrw_v2_recv_cb recv_cb);
int ctr_lrw_v2_start(void);
int ctr_lrw_v2_join(void);
int ctr_lrw_v2_send(const struct ctr_lrw_send_opts *opts, const void *buf, size_t len);
int ctr_lrw_v2_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LRW_V2_H_ */
