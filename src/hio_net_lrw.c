// TODO Implement retries settings parameter

#include <hio_net_lrw.h>
#include <hio_bsp.h>
#include <hio_config.h>
#include <hio_lrw_talk.h>
#include <hio_util.h>

// Zephyr includes
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <shell/shell.h>
#include <zephyr.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(hio_net_lrw, LOG_LEVEL_DBG);

#define SETTINGS_PFX "lrw"

#define BOOT_RETRY_COUNT 3
#define BOOT_RETRY_DELAY K_SECONDS(10)
#define SETUP_RETRY_COUNT 3
#define SETUP_RETRY_DELAY K_SECONDS(10)
#define JOIN_TIMEOUT K_SECONDS(30)
#define JOIN_RETRY_COUNT 3
#define JOIN_RETRY_DELAY K_SECONDS(30)
#define SEND_TIMEOUT K_SECONDS(120)
#define SEND_RETRY_COUNT 3
#define SEND_RETRY_DELAY K_SECONDS(30)

#define CMD_MSGQ_MAX_ITEMS 16
#define SEND_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
    CMD_MSGQ_REQ_START = 0,
    CMD_MSGQ_REQ_JOIN = 1
};

struct cmd_msgq_item {
    int corr_id;
    enum cmd_msgq_req req;
};

struct send_msgq_data {
    int64_t ttl;
    bool confirmed;
    int port;
    void *buf;
    size_t len;
};

struct send_msgq_item {
    int corr_id;
    struct send_msgq_data data;
};

static K_MUTEX_DEFINE(m_config_mut);

enum state {
    STATE_ERROR = -1,
    STATE_INIT = 0,
    STATE_READY = 1
};

enum antenna {
    ANTENNA_INT = 0,
    ANTENNA_EXT = 1
};

enum band {
    BAND_AS923 = 0,
    BAND_AU915 = 1,
    BAND_EU868 = 5,
    BAND_KR920 = 6,
    BAND_IN865 = 7,
    BAND_US915 = 8
};

enum class {
    CLASS_A = 0,
    CLASS_C = 2
};

enum mode {
    MODE_ABP = 0,
    MODE_OTAA = 1
};

enum nwk {
    NWK_PRIVATE = 0,
    NWK_PUBLIC = 1
};

struct config {
    enum antenna antenna;
    enum band band;
    enum class class;
    enum mode mode;
    enum nwk nwk;
    bool adr;
    bool dutycycle;
    uint8_t devaddr[4];
    uint8_t deveui[8];
    uint8_t joineui[8];
    uint8_t appkey[16];
    uint8_t nwkskey[16];
    uint8_t appskey[16];
};

static enum state m_state = STATE_INIT;

static struct config m_config = {
    .antenna = ANTENNA_INT,
    .band = BAND_EU868,
    .class = CLASS_A,
    .mode = MODE_OTAA,
    .nwk = NWK_PUBLIC,
    .adr = true,
    .dutycycle = true
};

static struct k_poll_signal m_boot_sig;
static struct k_poll_signal m_join_sig;
static struct k_poll_signal m_send_sig;

K_MSGQ_DEFINE(m_cmd_msgq, sizeof(struct cmd_msgq_item),
              CMD_MSGQ_MAX_ITEMS, 4);
K_MSGQ_DEFINE(m_send_msgq, sizeof(struct send_msgq_item),
              SEND_MSGQ_MAX_ITEMS, 4);

static hio_net_lrw_event_cb m_callback;
static void *m_param;
static atomic_t m_corr_id;

static int h_set(const char *key, size_t len,
                 settings_read_cb read_cb, void *cb_arg);

static int h_export(int (*export_func)(const char *name,
                                       const void *val, size_t val_len));

static void talk_handler(enum hio_lrw_talk_event event)
{
    switch (event) {
    case HIO_LRW_TALK_EVENT_BOOT:
        LOG_DBG("Event `HIO_LRW_TALK_EVENT_BOOT`");
        k_poll_signal_raise(&m_boot_sig, 0);
        break;
    case HIO_LRW_TALK_EVENT_JOIN_OK:
        LOG_DBG("Event `HIO_LRW_TALK_EVENT_JOIN_OK`");
        k_poll_signal_raise(&m_join_sig, 0);
        break;
    case HIO_LRW_TALK_EVENT_JOIN_ERR:
        LOG_DBG("Event `HIO_LRW_TALK_EVENT_JOIN_ERR`");
        k_poll_signal_raise(&m_join_sig, 1);
        break;
    case HIO_LRW_TALK_EVENT_SEND_OK:
        LOG_DBG("Event `HIO_LRW_TALK_EVENT_SEND_OK`");
        k_poll_signal_raise(&m_send_sig, 0);
        break;
    case HIO_LRW_TALK_EVENT_SEND_ERR:
        LOG_DBG("Event `HIO_LRW_TALK_EVENT_SEND_ERR`");
        k_poll_signal_raise(&m_send_sig, 1);
        break;
    default:
        LOG_WRN("Unknown event: %d", (int)event);
    }
}

static int boot_once(void)
{
    int ret;

    hio_bsp_set_lrw_reset(0);

    k_sleep(K_MSEC(100));

    k_poll_signal_reset(&m_boot_sig);

    hio_bsp_set_lrw_reset(1);

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                    K_POLL_MODE_NOTIFY_ONLY,
                                    &m_boot_sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(1000));

    if (ret == -EAGAIN) {
        LOG_WRN("Boot message timed out");
        return -ETIMEDOUT;
    }

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    return 0;
}

static int boot(int retries, k_timeout_t delay)
{
    int ret;

    LOG_INF("Operation BOOT started");

    while (retries-- > 0) {
        ret = hio_lrw_talk_enable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_enable` failed: %d", ret);
            return ret;
        }

        ret = boot_once();

        if (ret == 0) {
            ret = hio_lrw_talk_disable();

            if (ret < 0) {
                LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
                return ret;
            }

            LOG_INF("Operation BOOT succeeded");
            return 0;
        }

        if (ret < 0) {
            LOG_WRN("Call `boot_once` failed: %d", ret);
        }

        ret = hio_lrw_talk_disable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
            return ret;
        }

        if (retries > 0) {
            k_sleep(delay);
            LOG_WRN("Repeating BOOT operation (retries left: %d)", retries);
        }
    }

    LOG_ERR("Operation BOOT failed");
    return -ENOLINK;
}

static int setup_once(void)
{
    int ret;

    k_mutex_lock(&m_config_mut, K_FOREVER);

    struct config config;
    memcpy(&config, &m_config, sizeof(config));

    k_mutex_unlock(&m_config_mut);

    ret = hio_lrw_talk_at_dformat(1);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_dformat` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_band((uint8_t)config.band);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_band` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_class((uint8_t)config.class);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_class` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_mode((uint8_t)config.mode);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_mode` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_nwk((uint8_t)config.nwk);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_nwk` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_adr(config.adr ? 1 : 0);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_adr` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_dutycycle(config.dutycycle ? 1 : 0);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_dutycycle` failed: %d", ret);
        return ret;
    }

    if (config.mode != MODE_ABP) {
        ret = hio_lrw_talk_at_joindc(config.dutycycle ? 1 : 0);

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at_joindc` failed: %d", ret);
            return ret;
        }
    }

    ret = hio_lrw_talk_at_devaddr(config.devaddr, sizeof(config.devaddr));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_devaddr` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_deveui(config.deveui, sizeof(config.deveui));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_deveui` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_appeui(config.joineui, sizeof(config.joineui));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_appeui` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_appkey(config.appkey, sizeof(config.appkey));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_appkey` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_nwkskey(config.nwkskey, sizeof(config.nwkskey));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_nwkskey` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_at_appskey(config.appskey, sizeof(config.appskey));

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_appskey` failed: %d", ret);
        return ret;
    }

    return 0;
}

static int setup(int retries, k_timeout_t delay)
{
    int ret;

    LOG_INF("Operation SETUP started");

    while (retries-- > 0) {
        ret = hio_lrw_talk_enable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_enable` failed: %d", ret);
            return ret;
        }

        ret = setup_once();

        if (ret == 0) {
            ret = hio_lrw_talk_disable();

            if (ret < 0) {
                LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
                return ret;
            }

            LOG_INF("Operation SETUP succeeded");
            return 0;
        }

        if (ret < 0) {
            LOG_WRN("Call `setup_once` failed: %d", ret);
        }

        ret = hio_lrw_talk_disable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
            return ret;
        }

        if (retries > 0) {
            k_sleep(delay);
            LOG_WRN("Repeating SETUP operation (retries left: %d)", retries);
        }
    }

    LOG_ERR("Operation SETUP failed");
    return -EPIPE;
}

static int join_once(void)
{
    int ret;

    k_poll_signal_reset(&m_join_sig);

    ret = hio_lrw_talk_at_join();

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_at_join` failed: %d", ret);
        return ret;
    }

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &m_join_sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), JOIN_TIMEOUT);

    if (ret == -EAGAIN) {
        LOG_WRN("Join response timed out");
        return -ETIMEDOUT;
    }

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    unsigned int signaled;
    int result;

    k_poll_signal_check(&m_join_sig, &signaled, &result);

    if (signaled == 0 || result != 0) {
        return -ENOTCONN;
    }

    return 0;
}

static int join(int retries, k_timeout_t delay)
{
    int ret;

    LOG_INF("Operation JOIN started");

    while (retries-- > 0) {
        ret = hio_lrw_talk_enable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_enable` failed: %d", ret);
            return ret;
        }

        ret = join_once();

        if (ret == 0) {
            ret = hio_lrw_talk_disable();

            if (ret < 0) {
                LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
                return ret;
            }

            LOG_INF("Operation JOIN succeeded");
            return 0;
        }

        if (ret < 0) {
            LOG_WRN("Call `join_once` failed: %d", ret);
        }

        ret = hio_lrw_talk_disable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
            return ret;
        }

        if (retries > 0) {
            k_sleep(delay);
            LOG_WRN("Repeating JOIN operation (retries left: %d)", retries);
        }
    }

    LOG_WRN("Operation JOIN failed");
    return -ENOTCONN;
}

static int send_once(const struct send_msgq_data *data)
{
    int ret;

    if (!data->confirmed && data->port < 0) {
        ret = hio_lrw_talk_at_utx(data->buf, data->len);

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at_utx` failed: %d", ret);
            return ret;
        }

        k_poll_signal_raise(&m_send_sig, 0);

    } else if (data->confirmed && data->port < 0) {
        ret = hio_lrw_talk_at_ctx(data->buf, data->len);

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at_ctx` failed: %d", ret);
            return ret;
        }

        k_poll_signal_reset(&m_send_sig);

    } else if (!data->confirmed && data->port >= 0) {
        ret = hio_lrw_talk_at_putx(data->port, data->buf, data->len);

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at_putx` failed: %d", ret);
            return ret;
        }

        k_poll_signal_raise(&m_send_sig, 0);

    } else {
        ret = hio_lrw_talk_at_pctx(data->port, data->buf, data->len);

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at_pctx` failed: %d", ret);
            return ret;
        }

        k_poll_signal_reset(&m_send_sig);
    }

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &m_send_sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), SEND_TIMEOUT);

    if (ret == -EAGAIN) {
        LOG_WRN("Send response timed out");
        return -ETIMEDOUT;
    }

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    unsigned int signaled;
    int result;

    k_poll_signal_check(&m_send_sig, &signaled, &result);

    if (signaled == 0 || result != 0) {
        return -ENOMSG;
    }

    return 0;
}

static int send(int retries, k_timeout_t delay,
                const struct send_msgq_data *data)
{
    int ret;

    LOG_INF("Operation SEND started");

    while (retries-- > 0) {
        ret = hio_lrw_talk_enable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_enable` failed: %d", ret);
            return ret;
        }

        ret = send_once(data);

        if (ret == 0) {
            ret = hio_lrw_talk_disable();

            if (ret < 0) {
                LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
                return ret;
            }

            LOG_INF("Operation SEND succeeded");
            return 0;
        }

        if (ret < 0) {
            LOG_WRN("Call `send_once` failed: %d", ret);
        }

        ret = hio_lrw_talk_disable();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_disable` failed: %d", ret);
            return ret;
        }

        if (retries > 0) {
            k_sleep(delay);
            LOG_WRN("Repeating SEND operation (retries left: %d)", retries);
        }
    }

    LOG_WRN("Operation SEND failed");
    return -ENODATA;
}

static int start(void)
{
    int ret;

    k_mutex_lock(&m_config_mut, K_FOREVER);

    struct config config;
    memcpy(&config, &m_config, sizeof(config));

    k_mutex_unlock(&m_config_mut);

    if (config.antenna == ANTENNA_EXT) {
        ret = hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_EXT);
    } else {
        ret = hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT);
    }

    if (ret < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_ant` failed: %d", ret);
        return ret;
    }

    ret = hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LRW);

    if (ret < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_mux` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_init(talk_handler);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_init` failed: %d", ret);
        return ret;
    }

    ret = boot(BOOT_RETRY_COUNT, BOOT_RETRY_DELAY);

    if (ret < 0) {
        LOG_ERR("Call `boot` failed: %d", ret);
        return ret;
    }

    ret = setup(BOOT_RETRY_COUNT, BOOT_RETRY_DELAY);

    if (ret < 0) {
        LOG_ERR("Call `setup` failed: %d", ret);
        return ret;
    }

    return 0;
}

static int process_req_start(const struct cmd_msgq_item *item)
{
    int ret;

    union hio_net_lrw_event_data data = {0};

    ret = start();

    if (ret < 0) {
        LOG_ERR("Call `start` failed: %d", ret);

        data.start_err.corr_id = item->corr_id;

        if (m_callback != NULL) {
            m_callback(HIO_NET_LRW_EVENT_START_ERR, &data, m_param);
        }

        return ret;
    }

    m_state = STATE_READY;

    data.start_ok.corr_id = item->corr_id;

    if (m_callback != NULL) {
        m_callback(HIO_NET_LRW_EVENT_START_OK, &data, m_param);
    }

    return 0;
}

static int process_req_join(const struct cmd_msgq_item *item)
{
    int ret;

    union hio_net_lrw_event_data data = {0};

    ret = join(JOIN_RETRY_COUNT, JOIN_RETRY_DELAY);

    if (ret < 0) {
        LOG_ERR("Call `join` failed: %d", ret);

        data.join_err.corr_id = item->corr_id;

        if (m_callback != NULL) {
            m_callback(HIO_NET_LRW_EVENT_JOIN_ERR, &data, m_param);
        }

        return ret;
    }

    data.join_ok.corr_id = item->corr_id;

    if (m_callback != NULL) {
        m_callback(HIO_NET_LRW_EVENT_JOIN_OK, &data, m_param);
    }

    return 0;
}

static int process_req_send(const struct send_msgq_item *item)
{
    int ret;

    union hio_net_lrw_event_data data = {0};

    if (item->data.ttl != 0) {
        if (k_uptime_get() > item->data.ttl) {
            LOG_WRN("Message TTL expired");
            k_free(item->data.buf);
            return -ECANCELED;
        }
    }

    ret = send(SEND_RETRY_COUNT, SEND_RETRY_DELAY, &item->data);

    k_free(item->data.buf);

    if (ret < 0) {
        LOG_ERR("Call `send` failed: %d", ret);

        data.send_err.corr_id = item->corr_id;

        if (m_callback != NULL) {
            m_callback(HIO_NET_LRW_EVENT_SEND_ERR, &data, m_param);
        }

        return ret;
    }

    data.send_ok.corr_id = item->corr_id;

    if (m_callback != NULL) {
        m_callback(HIO_NET_LRW_EVENT_SEND_OK, &data, m_param);
    }

    return 0;
}

static void process_cmd_msgq(void)
{
    int ret;

    struct cmd_msgq_item item;

    ret = k_msgq_get(&m_cmd_msgq, &item, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_msgq_get` failed: %d", ret);
        return;
    }

    if (item.req == CMD_MSGQ_REQ_START) {
        LOG_INF("Dequeued START command (correlation id: %d)", item.corr_id);

        if (m_state != STATE_INIT && m_state != STATE_ERROR) {
            LOG_WRN("No reason for START operation - ignoring");

        } else {
            ret = process_req_start(&item);

            m_state = ret == 0 ? STATE_READY : STATE_ERROR;
        }
    } else if (item.req == CMD_MSGQ_REQ_JOIN) {
        LOG_INF("Dequeued JOIN command (correlation id: %d)", item.corr_id);

        if (m_state != STATE_READY) {
            LOG_WRN("Not ready for JOIN command - ignoring");

        } else {
            ret = process_req_join(&item);

            m_state = ret == 0 ? STATE_READY : STATE_ERROR;
        }
    } else {
        LOG_ERR("Unknown message: %d", (int)item.req);
    }
}

static void process_send_msgq(void)
{
    int ret;

    struct send_msgq_item item;

    ret = k_msgq_get(&m_send_msgq, &item, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_msgq_get` failed: %d", ret);
        return;
    }

    LOG_INF("Dequeued SEND command (correlation id: %d)", item.corr_id);

    ret = process_req_send(&item);

    m_state = ret == 0 || ret == -ECANCELED ? STATE_READY : STATE_ERROR;
}

static void dispatcher_thread(void)
{
    int ret;

    for (;;) {
        struct k_poll_event events[] = {
            K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
                                     K_POLL_MODE_NOTIFY_ONLY,
                                     &m_cmd_msgq),
            K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
                                     K_POLL_MODE_NOTIFY_ONLY,
                                     &m_send_msgq)
        };

        ret = k_poll(events, m_state != STATE_READY ? 1 : 2, K_FOREVER);

        if (ret < 0) {
            LOG_ERR("Call `k_poll` failed: %d", ret);
            k_sleep(K_SECONDS(5));
            continue;
        }

        if (events[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
            process_cmd_msgq();
        }

        if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
            if (m_state == STATE_READY) {
                process_send_msgq();
            }
        }
    }
}

K_THREAD_DEFINE(hio_net_lrw, CONFIG_HIO_NET_LRW_THREAD_STACK_SIZE,
                dispatcher_thread, NULL, NULL, NULL,
                CONFIG_HIO_NET_LRW_THREAD_PRIORITY, 0, 0);

int hio_net_lrw_init(hio_net_lrw_event_cb callback, void *param)
{
    m_callback = callback;
    m_param = param;

    return 0;
}

int hio_net_lrw_start(int *corr_id)
{
    int ret;

    struct cmd_msgq_item item = {
        .corr_id = (int)atomic_inc(&m_corr_id),
        .req = CMD_MSGQ_REQ_START
    };

    LOG_INF("Enqueing START command (correlation id: %d)", item.corr_id);

    if (corr_id != NULL) {
        *corr_id = item.corr_id;
    }

    ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_msgq_put` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_net_lrw_join(int *corr_id)
{
    int ret;

    struct cmd_msgq_item item = {
        .corr_id = (int)atomic_inc(&m_corr_id),
        .req = CMD_MSGQ_REQ_JOIN
    };

    if (corr_id != NULL) {
        *corr_id = item.corr_id;
    }

    LOG_INF("Enqueing JOIN command (correlation id: %d)", item.corr_id);

    ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_msgq_put` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_net_lrw_send(const struct hio_net_lrw_send_opts *opts,
                     const void *buf, size_t len, int *corr_id)
{
    int ret;

    void *p = k_malloc(len);

    if (p == NULL) {
        LOG_ERR("Call `k_malloc` failed");
        return -ENOMEM;
    }

    memcpy(p, buf, len);

    struct send_msgq_item item = {
        .corr_id = (int)atomic_inc(&m_corr_id),
        .data = {
            .ttl = opts->ttl,
            .confirmed = opts->confirmed,
            .port = opts->port,
            .buf = p,
            .len = len
        }
    };

    LOG_INF("Enqueing SEND command (correlation id: %d)", item.corr_id);

    if (corr_id != NULL) {
        *corr_id = item.corr_id;
    }

    ret = k_msgq_put(&m_send_msgq, &item, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_msgq_put` failed: %d", ret);
        k_free(p);
        return ret;
    }

    return 0;
}

static int h_set(const char *key, size_t len,
                 settings_read_cb read_cb, void *cb_arg)
{
    int ret;
    const char *next;

    LOG_DBG("key: %s len: %u", key, len);

#define SETTINGS_SET(_key, _var, _size)                                       \
    do {                                                                      \
        if (settings_name_steq(key, _key, &next) && !next) {                  \
            if (len != _size) {                                               \
                return -EINVAL;                                               \
            }                                                                 \
                                                                              \
            k_mutex_lock(&m_config_mut, K_FOREVER);                           \
            ret = read_cb(cb_arg, _var, len);                                 \
            k_mutex_unlock(&m_config_mut);                                    \
                                                                              \
            if (ret < 0) {                                                    \
                LOG_ERR("Call `read_cb` failed: %d", ret);                    \
                return ret;                                                   \
            }                                                                 \
                                                                              \
            return 0;                                                         \
        }                                                                     \
    } while (0)

    SETTINGS_SET("antenna", &m_config.antenna, sizeof(m_config.antenna));
    SETTINGS_SET("band", &m_config.band, sizeof(m_config.band));
    SETTINGS_SET("class", &m_config.class, sizeof(m_config.class));
    SETTINGS_SET("mode", &m_config.mode, sizeof(m_config.mode));
    SETTINGS_SET("nwk", &m_config.nwk, sizeof(m_config.nwk));
    SETTINGS_SET("adr", &m_config.adr, sizeof(m_config.adr));
    SETTINGS_SET("dutycycle", &m_config.dutycycle, sizeof(m_config.dutycycle));
    SETTINGS_SET("devaddr", m_config.devaddr, sizeof(m_config.devaddr));
    SETTINGS_SET("deveui", m_config.deveui, sizeof(m_config.deveui));
    SETTINGS_SET("joineui", m_config.joineui, sizeof(m_config.joineui));
    SETTINGS_SET("appkey", m_config.appkey, sizeof(m_config.appkey));
    SETTINGS_SET("nwkskey", m_config.nwkskey, sizeof(m_config.nwkskey));
    SETTINGS_SET("appskey", m_config.appskey, sizeof(m_config.appskey));

#undef SETTINGS_SET

    return -ENOENT;
}

static int h_export(int (*export_func)(const char *name,
                                       const void *val, size_t val_len))
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

#define EXPORT_FUNC(_key, _var, _size)                         \
    do {                                                       \
        (void)export_func(SETTINGS_PFX "/" _key, _var, _size); \
    } while (0)

    EXPORT_FUNC("antenna", &m_config.antenna, sizeof(m_config.antenna));
    EXPORT_FUNC("band", &m_config.band, sizeof(m_config.band));
    EXPORT_FUNC("class", &m_config.class, sizeof(m_config.class));
    EXPORT_FUNC("mode", &m_config.mode, sizeof(m_config.mode));
    EXPORT_FUNC("nwk", &m_config.nwk, sizeof(m_config.nwk));
    EXPORT_FUNC("adr", &m_config.adr, sizeof(m_config.adr));
    EXPORT_FUNC("dutycycle", &m_config.dutycycle, sizeof(m_config.dutycycle));
    EXPORT_FUNC("devaddr", m_config.devaddr, sizeof(m_config.devaddr));
    EXPORT_FUNC("deveui", m_config.deveui, sizeof(m_config.deveui));
    EXPORT_FUNC("joineui", m_config.joineui, sizeof(m_config.joineui));
    EXPORT_FUNC("appkey", m_config.appkey, sizeof(m_config.appkey));
    EXPORT_FUNC("nwkskey", m_config.nwkskey, sizeof(m_config.nwkskey));
    EXPORT_FUNC("appskey", m_config.appskey, sizeof(m_config.appskey));

#undef EXPORT_FUNC

    k_mutex_unlock(&m_config_mut);

    return 0;
}

static void print_antenna(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    switch (m_config.antenna) {
    case ANTENNA_INT:
        shell_print(shell, SETTINGS_PFX " config antenna int");
        break;
    case ANTENNA_EXT:
        shell_print(shell, SETTINGS_PFX " config antenna ext");
        break;
    default:
        shell_print(shell, SETTINGS_PFX " config antenna (unknown)");
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_band(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    switch (m_config.band) {
    case BAND_AS923:
        shell_print(shell, SETTINGS_PFX " config band as923");
        break;
    case BAND_AU915:
        shell_print(shell, SETTINGS_PFX " config band au915");
        break;
    case BAND_EU868:
        shell_print(shell, SETTINGS_PFX " config band eu868");
        break;
    case BAND_KR920:
        shell_print(shell, SETTINGS_PFX " config band kr920");
        break;
    case BAND_IN865:
        shell_print(shell, SETTINGS_PFX " config band in865");
        break;
    case BAND_US915:
        shell_print(shell, SETTINGS_PFX " config band us915");
        break;
    default:
        shell_print(shell, SETTINGS_PFX " config band (unknown)");
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_class(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    switch (m_config.class) {
    case CLASS_A:
        shell_print(shell, SETTINGS_PFX " config class a");
        break;
    case CLASS_C:
        shell_print(shell, SETTINGS_PFX " config class c");
        break;
    default:
        shell_print(shell, SETTINGS_PFX " config class (unknown)");
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_mode(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    switch (m_config.mode) {
    case MODE_ABP:
        shell_print(shell, SETTINGS_PFX " config mode abp");
        break;
    case MODE_OTAA:
        shell_print(shell, SETTINGS_PFX " config mode otaa");
        break;
    default:
        shell_print(shell, SETTINGS_PFX " config mode (unknown)");
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_nwk(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    switch (m_config.nwk) {
    case NWK_PRIVATE:
        shell_print(shell, SETTINGS_PFX " config nwk private");
        break;
    case NWK_PUBLIC:
        shell_print(shell, SETTINGS_PFX " config nwk public");
        break;
    default:
        shell_print(shell, SETTINGS_PFX " config nwk (unknown)");
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_adr(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);
    shell_print(shell, SETTINGS_PFX " config adr %s",
                m_config.adr ? "true" : "false");
    k_mutex_unlock(&m_config_mut);
}

static void print_dutycycle(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);
    shell_print(shell, SETTINGS_PFX " config dutycycle %s",
                m_config.dutycycle ? "true" : "false");
    k_mutex_unlock(&m_config_mut);
}

static void print_devaddr(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.devaddr) * 2 + 1];

    if (hio_buf2hex(m_config.devaddr, sizeof(m_config.devaddr),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config devaddr %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_deveui(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.deveui) * 2 + 1];

    if (hio_buf2hex(m_config.deveui, sizeof(m_config.deveui),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config deveui %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_joineui(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.joineui) * 2 + 1];

    if (hio_buf2hex(m_config.joineui, sizeof(m_config.joineui),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config joineui %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_appkey(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.appkey) * 2 + 1];

    if (hio_buf2hex(m_config.appkey, sizeof(m_config.appkey),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config appkey %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_nwkskey(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.nwkskey) * 2 + 1];

    if (hio_buf2hex(m_config.nwkskey, sizeof(m_config.nwkskey),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config nwkskey %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static void print_appskey(const struct shell *shell)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    char buf[sizeof(m_config.appskey) * 2 + 1];

    if (hio_buf2hex(m_config.appskey, sizeof(m_config.appskey),
                    buf, sizeof(buf), false) >= 0) {
        shell_print(shell, SETTINGS_PFX " config appskey %s", buf);
    }

    k_mutex_unlock(&m_config_mut);
}

static int cmd_config_show(const struct shell *shell,
                           size_t argc, char **argv)
{
    k_mutex_lock(&m_config_mut, K_FOREVER);

    print_antenna(shell);
    print_band(shell);
    print_class(shell);
    print_mode(shell);
    print_nwk(shell);
    print_adr(shell);
    print_dutycycle(shell);
    print_devaddr(shell);
    print_deveui(shell);
    print_joineui(shell);
    print_appkey(shell);
    print_nwkskey(shell);
    print_appskey(shell);

    k_mutex_unlock(&m_config_mut);

    return 0;
}

static int cmd_config_antenna(const struct shell *shell,
                              size_t argc, char **argv)
{
    if (argc == 1) {
        print_antenna(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "int") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.antenna = ANTENNA_INT;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "ext") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.antenna = ANTENNA_EXT;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_band(const struct shell *shell,
                           size_t argc, char **argv)
{
    if (argc == 1) {
        print_band(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "as923") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_AS923;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "au915") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_AU915;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "eu868") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_EU868;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "kr920") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_KR920;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "in865") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_IN865;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "us915") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.band = BAND_US915;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_class(const struct shell *shell,
                            size_t argc, char **argv)
{
    if (argc == 1) {
        print_class(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "a") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.class = CLASS_A;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "c") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.class = CLASS_C;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_mode(const struct shell *shell,
                           size_t argc, char **argv)
{
    if (argc == 1) {
        print_mode(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "otaa") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.mode = MODE_OTAA;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "abp") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.mode = MODE_ABP;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_nwk(const struct shell *shell,
                          size_t argc, char **argv)
{
    if (argc == 1) {
        print_nwk(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "private") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.nwk = NWK_PRIVATE;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "public") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.nwk = NWK_PUBLIC;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_adr(const struct shell *shell,
                          size_t argc, char **argv)
{
    if (argc == 1) {
        print_adr(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "true") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.adr = true;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "false") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.adr = false;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_dutycycle(const struct shell *shell,
                                size_t argc, char **argv)
{
    if (argc == 1) {
        print_dutycycle(shell);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "true") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.dutycycle = true;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "false") == 0) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        m_config.dutycycle = false;
        k_mutex_unlock(&m_config_mut);
        return 0;
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_devaddr(const struct shell *shell,
                              size_t argc, char **argv)
{
    if (argc == 1) {
        print_devaddr(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.devaddr,
                              sizeof(m_config.devaddr), true);
        k_mutex_unlock(&m_config_mut);

        if (ret == sizeof(m_config.devaddr)) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_deveui(const struct shell *shell,
                             size_t argc, char **argv)
{
    if (argc == 1) {
        print_deveui(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.deveui,
                              sizeof(m_config.deveui), true);
        k_mutex_unlock(&m_config_mut);

        if (ret == sizeof(m_config.deveui)) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_joineui(const struct shell *shell,
                              size_t argc, char **argv)
{
    if (argc == 1) {
        print_joineui(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.joineui,
                              sizeof(m_config.joineui), true);
        k_mutex_unlock(&m_config_mut);

        if (ret >= 0) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_appkey(const struct shell *shell,
                             size_t argc, char **argv)
{
    if (argc == 1) {
        print_appkey(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.appkey,
                              sizeof(m_config.appkey), true);
        k_mutex_unlock(&m_config_mut);

        if (ret >= 0) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_nwkskey(const struct shell *shell,
                              size_t argc, char **argv)
{
    if (argc == 1) {
        print_nwkskey(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.nwkskey,
                              sizeof(m_config.nwkskey), true);
        k_mutex_unlock(&m_config_mut);

        if (ret >= 0) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_config_appskey(const struct shell *shell,
                              size_t argc, char **argv)
{
    if (argc == 1) {
        print_appskey(shell);
        return 0;
    }

    if (argc == 2) {
        k_mutex_lock(&m_config_mut, K_FOREVER);
        int ret = hio_hex2buf(argv[1], m_config.appskey,
                              sizeof(m_config.appskey), true);
        k_mutex_unlock(&m_config_mut);

        if (ret >= 0) {
            return 0;
        }
    }

    shell_help(shell);
    return -EINVAL;
}

static int cmd_start(const struct shell *shell,
                     size_t argc, char **argv)
{
    int ret;

    if (argc > 1) {
        shell_error(shell, "command not found: %s", argv[1]);
        shell_help(shell);
        return -EINVAL;
    }

    int corr_id;

    ret = hio_net_lrw_start(&corr_id);

    if (ret < 0) {
        LOG_ERR("Call `hio_net_lrw_start` failed: %d", ret);
        shell_error(shell, "command failed");
        return ret;
    }

    shell_print(shell, "correlation id: %d", corr_id);

    return 0;
}

static int cmd_join(const struct shell *shell,
                    size_t argc, char **argv)
{
    int ret;

    if (argc > 1) {
        shell_error(shell, "command not found: %s", argv[1]);
        shell_help(shell);
        return -EINVAL;
    }

    int corr_id;

    ret = hio_net_lrw_join(&corr_id);

    if (ret < 0) {
        LOG_ERR("Call `hio_net_lrw_join` failed: %d", ret);
        shell_error(shell, "command failed");
        return ret;
    }

    shell_print(shell, "correlation id: %d", corr_id);

    return 0;
}

static int print_help(const struct shell *shell,
                      size_t argc, char **argv)
{
    if (argc > 1) {
        shell_error(shell, "command not found: %s", argv[1]);
        shell_help(shell);
        return -EINVAL;
    }

    shell_help(shell);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_lrw_config,
    SHELL_CMD_ARG(show, NULL,
                  "List current configuration.",
                  cmd_config_show, 1, 0),
    SHELL_CMD_ARG(antenna, NULL,
                  "Get/Set LoRaWAN antenna (format: <int|ext>).",
                  cmd_config_antenna, 1, 1),
    SHELL_CMD_ARG(band, NULL,
                  "Get/Set radio band "
                  "(format: <as923|au915|eu868|kr920|in865|us915>).",
                  cmd_config_band, 1, 1),
    SHELL_CMD_ARG(class, NULL,
                  "Get/Set device class (format: <a|c>).",
                  cmd_config_class, 1, 1),
    SHELL_CMD_ARG(mode, NULL,
                  "Get/Set operation mode (format: <abp|otaa>).",
                  cmd_config_mode, 1, 1),
    SHELL_CMD_ARG(nwk, NULL,
                  "Get/Set network type (format: <private|public>).",
                  cmd_config_nwk, 1, 1),
    SHELL_CMD_ARG(adr, NULL,
                  "Get/Set adaptive data rate (format: <true|false>).",
                  cmd_config_adr, 1, 1),
    SHELL_CMD_ARG(dutycycle, NULL,
                  "Get/Set duty cycle (format: <true|false>).",
                  cmd_config_dutycycle, 1, 1),
    SHELL_CMD_ARG(devaddr, NULL,
                  "Get/Set DevAddr (format: <8 hexadecimal digits>).",
                  cmd_config_devaddr, 1, 1),
    SHELL_CMD_ARG(deveui, NULL,
                  "Get/Set DevEUI (format: <16 hexadecimal digits>).",
                  cmd_config_deveui, 1, 1),
    SHELL_CMD_ARG(joineui, NULL,
                  "Get/Set JoinEUI (format: <16 hexadecimal digits>).",
                  cmd_config_joineui, 1, 1),
    SHELL_CMD_ARG(appkey, NULL,
                  "Get/Set AppKey (format: <32 hexadecimal digits>).",
                  cmd_config_appkey, 1, 1),
    SHELL_CMD_ARG(nwkskey, NULL,
                  "Get/Set NwkSKey (format: <32 hexadecimal digits>).",
                  cmd_config_nwkskey, 1, 1),
    SHELL_CMD_ARG(appskey, NULL,
                  "Get/Set AppSKey (format: <32 hexadecimal digits>).",
                  cmd_config_appskey, 1, 1),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_lrw,
    SHELL_CMD_ARG(config, &sub_lrw_config, "Configuration commands.",
                  print_help, 1, 0),
    SHELL_CMD_ARG(start, NULL, "Start interface.",
                  cmd_start, 1, 0),
    SHELL_CMD_ARG(join, NULL, "Join network.",
                  cmd_join, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lrw, &sub_lrw, "LoRaWAN commands.", print_help);

static int init(const struct device *dev)
{
    ARG_UNUSED(dev);

    int ret;

    LOG_INF("System initialization");

    k_poll_signal_init(&m_boot_sig);
    k_poll_signal_init(&m_join_sig);
    k_poll_signal_init(&m_send_sig);

    static struct settings_handler sh = {
        .name = SETTINGS_PFX,
        .h_set = h_set,
        .h_export = h_export
    };

    ret = settings_register(&sh);

    if (ret < 0) {
        LOG_ERR("Call `settings_register` failed: %d", ret);
        return ret;
    }

    ret = settings_load_subtree(SETTINGS_PFX);

    if (ret < 0) {
        LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
        return ret;
    }

    hio_config_append_show(SETTINGS_PFX, cmd_config_show);

    return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
