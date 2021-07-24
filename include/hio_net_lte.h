#ifndef HIO_NET_LTE_H
#define HIO_NET_LTE_H

#include <hio_sys.h>

// Standard includes
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool auto_connect;
    int plmn_id;
    int attach_retries;
    int64_t attach_pause;
    int send_retries;
    int64_t send_pause;
} hio_net_lte_cfg_t;

#define HIO_NET_LTE_CFG_DEFAULTS { \
    .auto_connect = true,          \
    .plmn_id = 23003,              \
    .attach_retries = 5,           \
    .attach_pause = 30 * 1000,     \
    .send_retries = 3,             \
    .send_pause = 10 * 1000        \
}

// TODO Change .port to 10000
#define HIO_NET_LTE_SEND_OPTS_DEFAULTS { \
    .ttl = HIO_SYS_FOREVER,              \
    .addr = {192, 168, 168, 1},          \
    .port = 7777                         \
}

typedef struct {
    enum {
        HIO_NET_LTE_EVENT_ATTACH_DONE = 0,
        HIO_NET_LTE_EVENT_ATTACH_ERROR = 1,
        HIO_NET_LTE_EVENT_DETACH_DONE = 2,
        HIO_NET_LTE_EVENT_DETACH_ERROR = 3,
        HIO_NET_LTE_EVENT_SEND_DONE = 4,
        HIO_NET_LTE_EVENT_SEND_ERROR = 5,
        HIO_NET_LTE_EVENT_RECV_DONE = 6
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
} hio_net_lte_event_t;

typedef struct {
    // Time-to-Live
    int64_t ttl;
    uint8_t addr[4];
    int port;
} hio_net_send_opts_t;

typedef struct {
    // Time-of-Arrival
    int64_t toa;
    uint8_t addr[4];
    int port;
    size_t len;
} hio_net_recv_info_t;

typedef void (*hio_net_lte_callback_t)(hio_net_lte_event_t *event, void *param);

int
hio_net_lte_init(const hio_net_lte_cfg_t *cfg);

int
hio_net_lte_set_callback(hio_net_lte_callback_t cb, void *param);

int
hio_net_lte_attach(void);

int
hio_net_lte_detach(void);

int
hio_net_lte_send(const hio_net_send_opts_t *opts, const void *buf, size_t len);

int
hio_net_lte_recv(hio_net_recv_info_t *info, void *buf, size_t size);

void
hio_net_lte_set_reg(bool state);

#endif
