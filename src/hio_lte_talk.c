#include <hio_lte_talk.h>
#include <hio_lte_uart.h>
#include <hio_log.h>

// Standard includes
#include <string.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "HIO/LTE/TALK"

int
hio_lte_talk_cmd(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int ret = hio_lte_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        hio_log_fatal("Call `hio_lte_uart_send` failed");
    }

    return 0;
}

char *
hio_lte_talk_rsp(hio_sys_timeout_t timeout)
{
    char *rsp = hio_lte_uart_recv(timeout);

    return rsp;
}

int
hio_lte_talk_ok(hio_sys_timeout_t timeout)
{
    char *rsp = hio_lte_uart_recv(timeout);

    if (rsp == NULL) {
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
    va_list ap;

    va_start(ap, fmt);
    int ret = hio_lte_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        return -1;
    }

    char *rsp = hio_lte_uart_recv(timeout);

    if (rsp == NULL) {
        return -2;
    }

    if (strcmp(rsp, "OK") != 0) {
        // TODO Remove
        for (size_t i = 0; i < strlen(rsp); i++) {
            hio_log_debug("rsp[%u]: %02x (%c)", i, rsp[i], rsp[i]);
        }

        return -3;
    }

    return 0;
}
