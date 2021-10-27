#ifndef CHESTER_INCLUDE_NET_LTE_H_
#define CHESTER_INCLUDE_NET_LTE_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_net_lte_event {
	CTR_NET_LTE_EVENT_FAILURE = -1,
	CTR_NET_LTE_EVENT_START_OK = 0,
	CTR_NET_LTE_EVENT_START_ERR = 1,
	CTR_NET_LTE_EVENT_ATTACH_OK = 2,
	CTR_NET_LTE_EVENT_ATTACH_ERR = 3,
	CTR_NET_LTE_EVENT_DETACH_OK = 4,
	CTR_NET_LTE_EVENT_DETACH_ERR = 5,
	CTR_NET_LTE_EVENT_SEND_OK = 6,
	CTR_NET_LTE_EVENT_SEND_ERR = 7,
};

struct ctr_net_lte_data_start_ok {
	int corr_id;
};

struct ctr_net_lte_data_start_err {
	int corr_id;
};

struct ctr_net_lte_data_attach_ok {
	int corr_id;
};

struct ctr_net_lte_data_attach_err {
	int corr_id;
};

struct ctr_net_lte_data_detach_ok {
	int corr_id;
};

struct ctr_net_lte_data_detach_err {
	int corr_id;
};

struct ctr_net_lte_data_send_ok {
	int corr_id;
};

struct ctr_net_lte_data_send_err {
	int corr_id;
};

union ctr_net_lte_event_data {
	struct ctr_net_lte_data_start_ok start_ok;
	struct ctr_net_lte_data_start_err start_err;
	struct ctr_net_lte_data_attach_ok attach_ok;
	struct ctr_net_lte_data_attach_err attach_err;
	struct ctr_net_lte_data_detach_ok detach_ok;
	struct ctr_net_lte_data_detach_err detach_err;
	struct ctr_net_lte_data_send_ok send_ok;
	struct ctr_net_lte_data_send_err send_err;
};

typedef void (*ctr_net_lte_event_cb)(enum ctr_net_lte_event event,
                                     union ctr_net_lte_event_data *data, void *param);

struct ctr_net_lte_send_opts {
	int64_t ttl;
	uint8_t addr[4];
	int port;
};

#define CTR_NET_LTE_SEND_OPTS_DEFAULTS                                                             \
	{                                                                                          \
		.ttl = CTR_SYS_FOREVER, .addr = { 192, 168, 168, 1 }, .port = 7777,                \
	}

int ctr_net_lte_init(ctr_net_lte_event_cb callback, void *param);
int ctr_net_lte_start(int *corr_id);
int ctr_net_lte_attach(int *corr_id);
int ctr_net_lte_detach(int *corr_id);
int ctr_net_lte_send(const struct ctr_net_lte_send_opts *opts, const void *buf, size_t len,
                     int *corr_id);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_NET_LTE_H_ */

#if 0

#ifndef CHESTER_INCLUDE_NET_LTE_H_
#define CHESTER_INCLUDE_NET_LTE_H_

#include <ctr_sys.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	bool auto_connect;
	int plmn_id;
	int attach_retries;
	int64_t attach_pause;
	int send_retries;
	int64_t send_pause;
} ctr_net_lte_cfg_t;

#define CTR_NET_LTE_CFG_DEFAULTS                                                                   \
	{                                                                                          \
		.auto_connect = true, .plmn_id = 23003, .attach_retries = 5,                       \
		.attach_pause = 30 * 1000, .send_retries = 3, .send_pause = 10 * 1000,             \
	}

/* TODO Change .port to 10000 */
#define CTR_NET_LTE_SEND_OPTS_DEFAULTS                                                             \
	{                                                                                          \
		.ttl = CTR_SYS_FOREVER, .addr = { 192, 168, 168, 1 }, .port = 7777,                \
	}

typedef struct {
	enum {
		CTR_NET_LTE_EVENT_ATTACH_DONE = 0,
		CTR_NET_LTE_EVENT_ATTACH_ERROR = 1,
		CTR_NET_LTE_EVENT_DETACH_DONE = 2,
		CTR_NET_LTE_EVENT_DETACH_ERROR = 3,
		CTR_NET_LTE_EVENT_SEND_DONE = 4,
		CTR_NET_LTE_EVENT_SEND_ERROR = 5,
		CTR_NET_LTE_EVENT_RECV_DONE = 6,
	} source;
	union {
		struct {
			const void *buf;
			size_t len;
		} send_done;
	} info;
	union {
		struct {
			bool stop;
		} attach_error;
		struct {
			bool stop;
		} detach_error;
		struct {
			bool stop;
		} send_error;
	} opts;
} ctr_net_lte_event_t;

typedef struct {
	/* Time-to-Live */
	int64_t ttl;
	uint8_t addr[4];
	int port;
} ctr_net_send_opts_t;

typedef struct {
	/* Time-of-Arrival */
	int64_t toa;
	uint8_t addr[4];
	int port;
	size_t len;
} ctr_net_recv_info_t;

typedef void (*ctr_net_lte_callback_t)(ctr_net_lte_event_t *event, void *param);

int ctr_net_lte_init(const ctr_net_lte_cfg_t *cfg);
int ctr_net_lte_set_callback(ctr_net_lte_callback_t cb, void *param);
int ctr_net_lte_attach(void);
int ctr_net_lte_detach(void);
int ctr_net_lte_send(const ctr_net_send_opts_t *opts, const void *buf, size_t len);
int ctr_net_lte_recv(ctr_net_recv_info_t *info, void *buf, size_t size);
void ctr_net_lte_set_reg(bool state);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_NET_LTE_H_ */

#endif
