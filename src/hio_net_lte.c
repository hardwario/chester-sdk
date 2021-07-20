#include <hio_net_lte.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_lte_talk.h>
#include <hio_lte_tok.h>
#include <hio_lte_uart.h>

// Standard includes
#include <stdint.h>
#include <stdio.h>
#include <string.h>

HIO_LOG_REGISTER(hio_net_lte, HIO_LOG_LEVEL_DBG);

#define SEND_MSGQ_MAX_ITEMS 16
#define RECV_MSGQ_MAX_ITEMS 16

#define SEND_MAX_LEN 512

#define TIMEOUT_S HIO_SYS_MSEC(5000)
#define TIMEOUT_L HIO_SYS_SECONDS(30)

typedef struct {
    int64_t ttl;
    int port;
    const void *buf;
    size_t len;
} send_item_t;

typedef struct {
    int64_t toa;
    int port;
    void *buf;
    size_t len;
} recv_item_t;

typedef enum {
    STATE_RESET = 0,
    STATE_ATTACHED = 1,
    STATE_DETACHED = 2
} state_t;

typedef struct {
    const hio_net_lte_cfg_t *cfg;
    hio_net_lte_callback_t cb;
    void *param;
    hio_sys_mut_t mut;
    hio_sys_sem_t sem;
    hio_sys_msgq_t send_msgq;
    char __aligned(4)
        send_msgq_mem[SEND_MSGQ_MAX_ITEMS * sizeof(send_item_t)];
    hio_sys_msgq_t recv_msgq;
    char __aligned(4)
        recv_msgq_mem[RECV_MSGQ_MAX_ITEMS * sizeof(recv_item_t)];
    hio_sys_task_t task;
    HIO_SYS_TASK_STACK_MEMBER(stack, 2048);
    state_t state_now;
    state_t state_req;
    bool registered;
    char buf[2 * SEND_MAX_LEN + 1];
} hio_net_lte_t;

static hio_net_lte_t inst;

static int
attach_once(void)
{
    hio_net_lte_t *ctx = &inst;

    if (hio_bsp_set_lte_reset(0) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_reset` failed [%p]", ctx);
        return -1;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_reset(1) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_reset` failed [%p]", ctx);
        return -2;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(1000));

    if (hio_bsp_set_lte_wkup(1) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_wkup` failed [%p]", ctx);
        return -3;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_wkup(0) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_wkup` failed [%p]", ctx);
        return -4;
    }

    char *rsp;

    if (hio_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_rsp` failed [%p]", ctx);
        return -5;
    }

    if (strcmp(rsp, "Ready") != 0) {
        hio_log_err("Boot message not received [%p]", ctx);
        return -6;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSYSTEMMODE=0,1,0,0") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -8;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CSCON=1") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -9;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEREG=%d", 5) < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -10;
    }

    const char *timer_t3412 = "00001000"; // T3412 Extended Timer
    const char *timer_t3324 = "00000010"; // T3324 Active Timer

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CPSMS=1,,,\"%s\",\"%s\"",
                            timer_t3412, timer_t3324) < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -11;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=1") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -12;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(1000));

    if (hio_lte_talk_cmd("AT+CIMI") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd` failed [%p]", ctx);
        return -13;
    }

    if (hio_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_rsp` failed [%p]", ctx);
        return -14;
    }

    if (hio_lte_talk_ok(TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_ok` failed [%p]", ctx);
        return -15;
    }

    int64_t end = hio_sys_uptime_get() + HIO_SYS_MINUTES(5);

    for (;;) {
        int64_t now = hio_sys_uptime_get();

        if (now >= end) {
            hio_log_wrn("Attach timed out [%p]", ctx);
            return -16;
        }

        if (hio_sys_sem_take(&ctx->sem, end - now) < 0) {
            continue;
        }

        hio_sys_mut_acquire(&ctx->mut);
        bool registered = ctx->registered;
        hio_sys_mut_release(&ctx->mut);

        if (registered) {
            break;
        }
    }

    hio_sys_sem_give(&ctx->sem);

    if (hio_lte_talk_cmd("AT#XSOCKET=1,2,0") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -17;
    }

    // TODO Short timeout?
    if (hio_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_rsp` failed [%p]", ctx);
        return -18;
    }

    if (strcmp(rsp, "#XSOCKET: 1,2,0,17") != 0) {
        hio_log_err("Unexpected response [%p]", ctx);
        return -19;
    }

    if (hio_lte_talk_ok(TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_ok` failed [%p]", ctx);
        return -20;
    }

    return 0;
}

static int
attach(void)
{
    hio_net_lte_t *ctx = &inst;

    hio_log_inf("Attach started [%p]", ctx);

    hio_sys_mut_acquire(&ctx->mut);
    int retries = ctx->cfg->attach_retries;
    int64_t pause = ctx->cfg->attach_pause;
    hio_sys_mut_release(&ctx->mut);

    if (retries <= 0) {
        hio_log_err("Parameter `retries` invalid [%p]", ctx);
        return -1;
    }

    do {
        if (attach_once() < 0) {
            goto error;
        }

        hio_log_inf("Attach succeeded [%p]", ctx);

        if (ctx->cb != NULL) {
            hio_net_lte_event_t event = {0};
            event.source = HIO_NET_LTE_EVENT_ATTACH_DONE;
            ctx->cb(&event, ctx->param);
        }

        return 0;

error:
        hio_log_wrn("Attach failed [%p]", ctx);

        if (ctx->cb != NULL) {
            hio_net_lte_event_t event = {0};
            event.source = HIO_NET_LTE_EVENT_ATTACH_ERROR;
            ctx->cb(&event, ctx->param);
            if (event.opts.attach_error.stop) {
                hio_log_inf("Attach cancelled [%p]", ctx);
                return -2;
            }
        }

        hio_sys_task_sleep(pause);

    } while (--retries > 0);

    return -3;
}

static int
detach(void)
{
    hio_net_lte_t *ctx = &inst;

    hio_log_inf("Detach started [%p]", ctx);

    int ret = 0;

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        ret = -1;
        goto error;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT#XSLEEP=2") < 0) {
        hio_log_err("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        ret = -2;
        goto error;
    }

    hio_log_inf("Detach succeeded [%p]", ctx);

    if (ctx->cb != NULL) {
        hio_net_lte_event_t event = {0};
        event.source = HIO_NET_LTE_EVENT_DETACH_DONE;
        ctx->cb(&event, ctx->param);
    }

    return 0;

error:
    hio_log_wrn("Detach failed [%p]", ctx);

    if (ctx->cb != NULL) {
        hio_net_lte_event_t event = {0};
        event.source = HIO_NET_LTE_EVENT_DETACH_ERROR;
        ctx->cb(&event, ctx->param);
    }

    return ret;
}

static state_t
check_state(void)
{
    hio_net_lte_t *ctx = &inst;

    hio_sys_mut_acquire(&ctx->mut);
    state_t state_now = ctx->state_now;
    state_t state_req = ctx->state_req;
    hio_sys_mut_release(&ctx->mut);

    if (state_now != state_req) {
        switch (state_req) {
        case STATE_ATTACHED:
            if (attach() < 0) {
                hio_log_wrn("Call `attach` failed [%p]", ctx);
            } else {
                state_now = STATE_ATTACHED;

                hio_sys_mut_acquire(&ctx->mut);
                ctx->state_now = state_now;
                hio_sys_mut_release(&ctx->mut);
            }
            break;
        case STATE_DETACHED:
            if (detach() < 0) {
                hio_log_wrn("Call `detach` failed [%p]", ctx);
            } else {
                state_now = STATE_DETACHED;

                hio_sys_mut_acquire(&ctx->mut);
                ctx->state_now = state_now;
                hio_sys_mut_release(&ctx->mut);
            }
            break;
        default:
            hio_log_err("Invalid state requested [%p]", ctx);
            break;
        }
    }

    return state_now;
}

static int
hexify(char *dst, size_t size, const void *src, size_t len)
{
    if (size < len * 2 + 1) {
        return -1;
    }

    memset(dst, 0, size);

    for (uint8_t *p = (uint8_t *) src; len != 0; p++, len--) {
        *dst++ = (*p >> 4) >= 10 ? (*p >> 4) - 10 + 'A' : (*p >> 4) + '0';
        *dst++ = (*p & 15) >= 10 ? (*p & 15) - 10 + 'A' : (*p & 15) + '0';
    }

    return 0;
}

static int
send_once(send_item_t *item)
{
    hio_net_lte_t *ctx = &inst;

    if (hexify(ctx->buf, sizeof(ctx->buf), item->buf, item->len) < 0) {
        hio_log_err("Call `hexify` failed [%p]", ctx);
        return -1;
    }

    if (hio_lte_talk_cmd("AT#XSENDTO=\"%u.%u.%u.%u\",%u,0,\"%s\"",
                         192, 168, 168, 1, item->port, ctx->buf) < 0) {
        hio_log_err("Call `hio_lte_talk_cmd` failed [%p]", ctx);
        return -2;
    }

    char *rsp;

    if (hio_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_rsp` failed [%p]", ctx);
        return -3;
    }


    if ((rsp = hio_lte_tok_pfx(rsp, "#XSENDTO: ")) == NULL) {
        hio_log_err("Call `hio_lte_tok_pfx` failed [%p]", ctx);
        return -4;
    }

    bool def;
    long num;

    if ((rsp = hio_lte_tok_num(rsp, &def, &num)) == NULL) {
        hio_log_err("Call `hio_lte_tok_num` failed [%p]", ctx);
        return -5;
    }

    if (!def || num != item->len) {
        hio_log_err("Number of sent bytes does not match [%p]", ctx);
        return -6;
    }

    if ((rsp = hio_lte_tok_end(rsp)) == NULL) {
        hio_log_err("Call `hio_lte_tok_end` failed [%p]", ctx);
        return -7;
    }

    if (hio_lte_talk_ok(TIMEOUT_S) < 0) {
        hio_log_err("Call `hio_lte_talk_ok` failed [%p]", ctx);
        return -8;
    }

    return 0;
}

static int
send(send_item_t *item)
{
    hio_net_lte_t *ctx = &inst;

    hio_log_inf("Send started [%p]", ctx);

    hio_sys_mut_acquire(&ctx->mut);
    int retries = ctx->cfg->send_retries;
    int64_t pause = ctx->cfg->send_pause;
    hio_sys_mut_release(&ctx->mut);

    if (retries <= 0) {
        hio_log_err("Parameter `retries` invalid [%p]", ctx);
        return -1;
    }

    if (hio_sys_uptime_get() > item->ttl) {
        hio_log_wrn("Message TTL expired [%p]", ctx);
        return -2;
    }

    do {
        if (send_once(item) < 0) {
            goto error;
        }

        hio_log_inf("Send succeeded [%p]", ctx);

        if (ctx->cb != NULL) {
            hio_net_lte_event_t event = {0};
            event.source = HIO_NET_LTE_EVENT_SEND_DONE;
            event.info.send_done.buf = item->buf;
            event.info.send_done.len = item->len;
            ctx->cb(&event, ctx->param);
        }

        return 0;

error:
        hio_log_wrn("Send failed [%p]", ctx);

        if (ctx->cb != NULL) {
            hio_net_lte_event_t event = {0};
            event.source = HIO_NET_LTE_EVENT_SEND_ERROR;
            ctx->cb(&event, ctx->param);
            if (event.opts.send_error.stop) {
                hio_log_inf("Send cancelled [%p]", ctx);
                return -3;
            }
        }

        if (attach() < 0) {
            hio_log_wrn("Call `attach` failed");
            return -4;
        }

        hio_sys_task_sleep(pause);

    } while (--retries > 0);

    return -5;
}

static void
check_send(void)
{
    hio_net_lte_t *ctx = &inst;

    for (;;) {
        send_item_t item;

        if (hio_sys_msgq_get(&ctx->send_msgq, &item, HIO_SYS_NO_WAIT) < 0) {
            break;
        }

        hio_log_inf("Dequeued message to send (port %d, len %u) [%p]",
                     item.port, item.len, ctx);

        if (send(&item) < 0) {
            hio_log_err("Call `send` failed [%p]", ctx);
        }
    }
}

static void
entry(void *param)
{
    hio_net_lte_t *ctx = param;

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        hio_log_fat("Call `hio_bsp_set_rf_ant` failed [%p]", ctx);
        return;
    }

    if (hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LTE) < 0) {
        hio_log_fat("Call `hio_bsp_set_rf_mux` failed [%p]", ctx);
        return;
    }

    if (hio_lte_uart_init() < 0) {
        hio_log_fat("Call `hio_lte_uart_init` failed [%p]", ctx);
        return;
    }

    for (;;) {
        if (hio_sys_sem_take(&ctx->sem, HIO_SYS_FOREVER) < 0) {
            hio_log_fat("Call `hio_sys_sem_take` failed [%p]", ctx);
            return;
        }

        state_t state = check_state();

        if (state == STATE_ATTACHED) {
            check_send();
        }
    }
}

int
hio_net_lte_init(const hio_net_lte_cfg_t *cfg)
{
    hio_net_lte_t *ctx = &inst;

    memset(ctx, 0, sizeof(*ctx));

    ctx->cfg = cfg;

    hio_sys_mut_init(&ctx->mut);
    hio_sys_sem_init(&ctx->sem, 0);
    hio_sys_msgq_init(&ctx->send_msgq, ctx->send_msgq_mem,
                      sizeof(send_item_t), SEND_MSGQ_MAX_ITEMS);
    hio_sys_msgq_init(&ctx->recv_msgq, ctx->recv_msgq_mem,
                      sizeof(recv_item_t), RECV_MSGQ_MAX_ITEMS);
    hio_sys_task_init(&ctx->task, "hio_net_lte", ctx->stack,
                      HIO_SYS_TASK_STACK_SIZEOF(ctx->stack), entry, ctx);

    return 0;
}

int
hio_net_lte_set_callback(hio_net_lte_callback_t cb, void *param)
{
    hio_net_lte_t *ctx = &inst;

    ctx->cb = cb;
    ctx->param = param;

    return 0;
}

int
hio_net_lte_attach(void)
{
    hio_net_lte_t *ctx = &inst;

    hio_sys_mut_acquire(&ctx->mut);
    ctx->state_req = STATE_ATTACHED;
    hio_sys_mut_release(&ctx->mut);

    hio_sys_sem_give(&ctx->sem);

    return 0;
}

int
hio_net_lte_detach(void)
{
    hio_net_lte_t *ctx = &inst;

    hio_sys_mut_acquire(&ctx->mut);
    ctx->state_req = STATE_DETACHED;
    hio_sys_mut_release(&ctx->mut);

    hio_sys_sem_give(&ctx->sem);

    return 0;
}

int
hio_net_lte_send(const hio_net_send_opts_t *opts, const void *buf, size_t len)
{
    hio_net_lte_t *ctx = &inst;

    send_item_t item = {
        .ttl = opts->ttl,
        .port = opts->port,
        .buf = buf,
        .len = len
    };

    if (hio_sys_msgq_put(&ctx->send_msgq, &item, HIO_SYS_NO_WAIT) < 0) {
        hio_log_err("Call `hio_sys_msgq_put` failed [%p]", ctx);
        return -1;
    }

    hio_sys_sem_give(&ctx->sem);

    return 0;
}

int
hio_net_lte_recv(hio_net_recv_info_t *info, void *buf, size_t size)
{
    return 0;
}

void
hio_net_lte_set_reg(bool state)
{
    hio_net_lte_t *ctx = &inst;

    hio_sys_mut_acquire(&ctx->mut);
    ctx->registered = state;
    hio_sys_mut_release(&ctx->mut);

    hio_sys_sem_give(&ctx->sem);
}
