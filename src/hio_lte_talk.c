#include <hio_lte_talk.h>
#include <hio_lte_tok.h>
#include <hio_lte_uart.h>
#include <hio_log.h>
#include <hio_net_lte.h>
#include <hio_sys.h>

// Standard includes
#include <string.h>

HIO_LOG_REGISTER(hio_lte_talk, HIO_LOG_LEVEL_DBG);

#define SEND_GUARD_TIME HIO_SYS_MSEC(100)

int
hio_lte_talk_cmd(const char *fmt, ...)
{
    hio_sys_task_sleep(SEND_GUARD_TIME);

    va_list ap;

    va_start(ap, fmt);
    int ret = hio_lte_uart_send(fmt, ap);
    va_end(ap);

    if (ret < 0) {
        hio_log_fat("Call `hio_lte_uart_send` failed");
        return -1;
    }

    return 0;
}

int
hio_lte_talk_rsp(char **s, int64_t timeout)
{
    if (hio_lte_uart_recv(s, timeout) < 0) {
        hio_log_err("Call `hio_lte_uart_recv` failed");
        return -1;
    }

    return 0;
}

int
hio_lte_talk_ok(int64_t timeout)
{
    char *rsp;

    if (hio_lte_uart_recv(&rsp, timeout) < 0) {
        hio_log_err("Call `hio_lte_uart_recv` failed");
        return -1;
    }

    if (strcmp(rsp, "OK") != 0) {
        return -2;
    }

    return 0;
}

int
hio_lte_talk_cmd_ok(int64_t timeout, const char *fmt, ...)
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
        hio_log_err("Call `hio_lte_uart_recv` failed");
        return -2;
    }

    if (strcmp(rsp, "OK") != 0) {
        hio_log_err("Response mismatch");
        return -3;
    }

    return 0;
}

void
hio_lte_talk_cereg(const char *s)
{
    char *p = (char *) s;

    if ((p = hio_lte_tok_pfx(p, "+CEREG: ")) == NULL) {
        return;
    }

    bool def_stat;
    long stat;

    if ((p = hio_lte_tok_num(p, &def_stat, &stat)) == NULL) {
        return;
    }

    /*
    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_str(p, NULL, NULL, 0)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_str(p, NULL, NULL, 0)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_num(p, NULL, NULL)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_num(p, NULL, NULL)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_num(p, NULL, NULL)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_str(p, NULL, NULL, 0)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_sep(p)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_str(p, NULL, NULL, 0)) == NULL) {
        return;
    }

    if ((p = hio_lte_tok_end(p)) == NULL) {
        return;
    }
    */

    if (!def_stat) {
        return;
    }

    hio_log_dbg("Parsed +CEREG");

    hio_net_lte_set_reg(stat == 1 || stat == 5 ? true : false);

    //+CEREG: 5,"B414","000F6E21",9,,,"00000010","00001000"
    //+CEREG: 2,"B414","000F6E21",9

    // TODO Process here
}
