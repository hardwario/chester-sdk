#include <hio_lte_talk.h>
#include <hio_lte_uart.h>
#include <hio_log.h>
#include <hio_sys.h>

// Standard includes
#include <string.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "HIO:LTE:TALK"

#define SEND_GUARD_TIME HIO_SYS_MSEC(1000)

int
hio_lte_talk_cmd(const char *fmt, ...)
{
    hio_sys_task_sleep(SEND_GUARD_TIME);

    va_list ap;

    va_start(ap, fmt);
    int ret = hio_lte_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        hio_log_fatal("Call `hio_lte_uart_send` failed");
        return -1;
    }

    return 0;
}

int
hio_lte_talk_rsp(char **s, hio_sys_timeout_t timeout)
{
    if (hio_lte_uart_recv(s, timeout) < 0) {
        hio_log_error("Call `hio_lte_uart_recv` failed");
        return -1;
    }

    return 0;
}

int
hio_lte_talk_ok(hio_sys_timeout_t timeout)
{
    char *rsp;

    if (hio_lte_uart_recv(&rsp, timeout) < 0) {
        hio_log_error("Call `hio_lte_uart_recv` failed");
        return -1;
    }

    if (strcmp(rsp, "OK") != 0) {
        return -2;
    }

    return 0;
}

int
hio_lte_talk_cmd_ok(hio_sys_timeout_t timeout, const char *fmt, ...)
{
    hio_sys_task_sleep(SEND_GUARD_TIME);

    va_list ap;

    va_start(ap, fmt);
    int ret = hio_lte_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        return -1;
    }

    char *rsp;

    if (hio_lte_uart_recv(&rsp, timeout) < 0) {
        hio_log_error("Call `hio_lte_uart_recv` failed");
        return -2;
    }

    if (strcmp(rsp, "OK") != 0) {
        hio_log_error("Response mismatch");
        return -3;
    }

    return 0;
}
