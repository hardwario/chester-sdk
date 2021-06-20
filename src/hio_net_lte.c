#include <hio_net_lte.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_lte_talk.h>
#include <hio_lte_uart.h>

// Standard includes
#include <string.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "HIO:NET:LTE"

#define SEND_MSGQ_MAX_ITEMS 16
#define RECV_MSGQ_MAX_ITEMS 16

#define TIMEOUT_S HIO_SYS_MSEC(5000)

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
    HIO_SYS_TASK_STACK_MEMBER(stack, 1024);
    state_t state_now;
    state_t state_req;
} hio_net_lte_t;

static hio_net_lte_t inst;

static int
attach(int retries, hio_sys_timeout_t delay)
{
    hio_log_info("Trying to attach");

    hio_net_lte_t *ctx = &inst;

    if (retries <= 0) {
        hio_log_error("Parameter `retries` invalid [%p]", ctx);
        return -1;
    }

    int ret;

    do {
        ret = 0;

        if (hio_bsp_set_lte_wkup(1) < 0) {
            hio_log_error("Call `hio_bsp_set_lte_wkup` failed [%p]", ctx);
            ret = -2;
            continue;
        }

        hio_sys_task_sleep(HIO_SYS_MSEC(10));

        if (hio_bsp_set_lte_wkup(0) < 0) {
            hio_log_error("Call `hio_bsp_set_lte_wkup` failed [%p]", ctx);
            ret = -3;
            continue;
        }

        hio_sys_task_sleep(HIO_SYS_MSEC(1000));

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0") < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -4;
            continue;
        }

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSYSTEMMODE=0,1,0,0") < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -5;
            continue;
        }

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CSCON=1") < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -6;
            continue;
        }

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEREG=%d", 5) < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -7;
            continue;
        }

        const char *timer_t3412 = "00001000"; // T3412 Extended Timer
        const char *timer_t3324 = "00000010"; // T3324 Active Timer

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CPSMS=1,,,\"%s\",\"%s\"",
                                timer_t3412, timer_t3324) < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -8;
            continue;
        }

        if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=1") < 0) {
            hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
            ret = -9;
            continue;
        }

        // TODO Implement wait for connection
        hio_sys_task_sleep(HIO_SYS_SECONDS(10));
        break;

        hio_sys_task_sleep(delay);

    } while (--retries > 0);

    return ret;
}

static int
detach(void)
{
    hio_log_info("Trying to detach");

    hio_net_lte_t *ctx = &inst;

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0") < 0) {
        hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -1;
    }

    if (hio_lte_talk_cmd_ok(TIMEOUT_S, "AT#XSLEEP=2") < 0) {
        hio_log_error("Call `hio_lte_talk_cmd_ok` failed [%p]", ctx);
        return -2;
    }

    return 0;
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
            if (attach(3, HIO_SYS_SECONDS(30)) < 0) {
                hio_log_warn("Call `attach` failed [%p]", ctx);
            } else {
                state_now = STATE_ATTACHED;

                hio_sys_mut_acquire(&ctx->mut);
                ctx->state_now = state_now;
                hio_sys_mut_release(&ctx->mut);

                ctx->cb(HIO_NET_LTE_EVENT_ATTACHED, ctx->param);
            }
            break;
        case STATE_DETACHED:
            if (detach() < 0) {
                hio_log_warn("Call `detach` failed [%p]", ctx);
            } else {
                state_now = STATE_DETACHED;

                hio_sys_mut_acquire(&ctx->mut);
                ctx->state_now = state_now;
                hio_sys_mut_release(&ctx->mut);

                ctx->cb(HIO_NET_LTE_EVENT_DETACHED, ctx->param);
            }
            break;
        default:
            hio_log_error("Invalid state requested [%p]", ctx);
            break;
        }
    }

    return state_now;
}

static void
check_send(void)
{
    hio_net_lte_t *ctx = &inst;

    send_item_t item;

    if (hio_sys_msgq_get(&ctx->send_msgq, &item, HIO_SYS_NO_WAIT) >= 0) {
        hio_log_info("Dequeued message to sent (port %d, len %u)", item.port, item.len);
    }
}

static void
entry(void *param)
{
    hio_net_lte_t *ctx = param;

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        hio_log_fatal("Call `hio_bsp_set_rf_ant` failed [%p]", ctx);
        return;
    }

    if (hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LTE) < 0) {
        hio_log_fatal("Call `hio_bsp_set_rf_mux` failed [%p]", ctx);
        return;
    }

    if (hio_lte_uart_init() < 0) {
        hio_log_fatal("Call `hio_lte_uart_init` failed [%p]", ctx);
        return;
    }

    for (;;) {

        if (hio_sys_sem_take(&ctx->sem, HIO_SYS_FOREVER) < 0) {
            hio_log_fatal("Call `hio_sys_sem_take` failed [%p]", ctx);
            return;
        }

        state_t state = check_state();

        if (state == STATE_ATTACHED) {
            check_send();
        }

        /*

        char *rsp;

        TRY(hio_lte_talk_cmd("AT#XPING=\"192.168.168.1\",50,5000"))
        TRY(hio_lte_talk_rsp(&rsp, TIMEOUT_S))
        TRY(hio_lte_talk_ok(TIMEOUT_S))

        TRY(hio_lte_talk_cmd("AT#XSLEEP=1"))
        TRY(hio_lte_talk_rsp(&rsp, HIO_SYS_MSEC(5000)))

        if (strcmp(rsp, "Ready") != 0) {
            goto error;
        }

        */
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
    hio_sys_task_init(&ctx->task, ctx->stack,
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
        hio_log_error("Call `hio_sys_msgq_put` failed");
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
