#include <hio_lrw_talk.h>
//#include <hio_lrw_tok.h>
#include <hio_lrw_uart.h>
#include <hio_util.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

// Standard includes
#include <stdbool.h>
#include <string.h>

LOG_MODULE_REGISTER(hio_lrw_talk, LOG_LEVEL_DBG);

#define SEND_GUARD_TIME K_MSEC(100)
#define RESPONSE_TIMEOUT_S K_MSEC(1000)

typedef bool (*handler_cb)(const char *s, void *param);

static handler_cb m_handler_cb;
static K_MUTEX_DEFINE(m_handler_mut);
static K_MUTEX_DEFINE(m_talk_mut);
static struct k_poll_signal m_response_sig;
static void *m_handler_param;

static void process_urc(const char *s)
{
}

static void recv_cb(const char *s)
{
    k_mutex_lock(&m_handler_mut, K_FOREVER);

    if (m_handler_cb != NULL && m_handler_cb(s, m_handler_param)) {
        k_mutex_unlock(&m_handler_mut);
        LOG_INF("RSP: %s", s);
        k_poll_signal_raise(&m_response_sig, 0);

    } else {
        k_mutex_unlock(&m_handler_mut);
        LOG_INF("URC: %s", s);
        process_urc(s);
    }
}

static int wait_response(k_timeout_t timeout, handler_cb cb, void *param)
{
    int ret;

    k_poll_signal_reset(&m_response_sig);

    k_mutex_lock(&m_handler_mut, K_FOREVER);
    m_handler_cb = cb;
    m_handler_param = param;
    k_mutex_unlock(&m_handler_mut);

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &m_response_sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), timeout);

    k_mutex_lock(&m_handler_mut, K_FOREVER);
    m_handler_cb = NULL;
    m_handler_param = NULL;
    k_mutex_unlock(&m_handler_mut);

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    return 0;
}

static bool ok_handler(const char *s, void *param)
{
    ARG_UNUSED(param);

    if (strcmp(s, "+OK") != 0) {
        return false;
    }

    return true;
}

static int talk_cmd_ok(k_timeout_t timeout, const char *fmt, ...)
{
    int ret;

    k_mutex_lock(&m_talk_mut, K_FOREVER);
    k_sleep(SEND_GUARD_TIME);

    va_list ap;

    va_start(ap, fmt);
    ret = hio_lrw_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_uart_send` failed: %d", ret);
        k_mutex_unlock(&m_talk_mut);
        return ret;
    }

    ret = wait_response(timeout, ok_handler, NULL);

    if (ret < 0) {
        LOG_ERR("Call `wait_response` failed: %d", ret);
        k_mutex_unlock(&m_talk_mut);
        return ret;
    }

    k_mutex_unlock(&m_talk_mut);
    return 0;
}

int hio_lrw_talk_init(void)
{
    int ret;

    k_poll_signal_init(&m_response_sig);

    ret = hio_lrw_uart_init(recv_cb);

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_uart_init` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_enable(void)
{
    int ret;

    ret = hio_lrw_uart_enable();

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_uart_enable` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_disable(void)
{
    int ret;

    ret = hio_lrw_uart_disable();

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_uart_disable` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at(void)
{
    int ret;

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT");

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_band(uint8_t band)
{
    int ret;

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+BAND=%u", band);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_class(uint8_t class)
{
    int ret;

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CLASS=%u", class);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_mode(uint8_t mode)
{
    int ret;

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+MODE=%u", mode);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_nwk(uint8_t nwk)
{
    int ret;

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+NWK=%u", nwk);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_devaddr(const uint8_t *devaddr, size_t devaddr_size)
{
    int ret;

    char hex[devaddr_size * 2 + 1];

    ret = hio_buf2hex(devaddr, devaddr_size, hex, sizeof(hex), true);

    if (ret != devaddr_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DEVADDR=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_deveui(const uint8_t *deveui, size_t deveui_size)
{
    int ret;

    char hex[deveui_size * 2 + 1];

    ret = hio_buf2hex(deveui, deveui_size, hex, sizeof(hex), true);

    if (ret != deveui_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DEVEUI=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_appeui(const uint8_t *appeui, size_t appeui_size)
{
    int ret;

    char hex[appeui_size * 2 + 1];

    ret = hio_buf2hex(appeui, appeui_size, hex, sizeof(hex), true);

    if (ret != appeui_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPEUI=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_appkey(const uint8_t *appkey, size_t appkey_size)
{
    int ret;

    char hex[appkey_size * 2 + 1];

    ret = hio_buf2hex(appkey, appkey_size, hex, sizeof(hex), true);

    if (ret != appkey_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPKEY=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_nwkskey(const uint8_t *nwkskey, size_t nwkskey_size)
{
    int ret;

    char hex[nwkskey_size * 2 + 1];

    ret = hio_buf2hex(nwkskey, nwkskey_size, hex, sizeof(hex), true);

    if (ret != nwkskey_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+NWKSKEY=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_lrw_talk_at_appskey(const uint8_t *appskey, size_t appskey_size)
{
    int ret;

    char hex[appskey_size * 2 + 1];

    ret = hio_buf2hex(appskey, appskey_size, hex, sizeof(hex), true);

    if (ret != appskey_size * 2) {
        LOG_ERR("Call `hio_buf2hex` failed: %d", ret);
        return ret;
    }

    ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPSKEY=%s", hex);

    if (ret < 0) {
        LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
        return ret;
    }

    return 0;
}
