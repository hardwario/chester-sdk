#include <hio_net_lte.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_lte_talk.h>
#include <hio_lte_uart.h>

// Standard includes
#include <string.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "HIO:NET:LTE"

struct hio_net_lte {
    const hio_net_lte_cfg_t *cfg;
    hio_net_lte_callback_t cb;
    void *param;
    hio_sys_sem_t sem;
    hio_sys_task_t task;
    HIO_SYS_TASK_STACK_MEMBER(stack, 1024);
};

hio_net_lte_t *
hio_net_lte_get_instance(void)
{
    static hio_net_lte_t inst;
    return &inst;
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

    for (int i = 0;; i++) {

        hio_log_info("Loop %d", i);

        hio_bsp_set_led(HIO_BSP_LED_R, true);
        hio_bsp_set_lte_wkup(1);
        hio_sys_task_sleep(HIO_SYS_MSEC(10));
        hio_bsp_set_lte_wkup(0);
        hio_bsp_set_led(HIO_BSP_LED_R, false);

        hio_sys_task_sleep(HIO_SYS_MSEC(5000));

        char *rsp;

#define TRY(exp) { int ret = exp; if (ret < 0) { hio_log_error("Sequence failed (%d) at line %d\n", ret, __LINE__); goto error; } }
#define TIMEOUT_S HIO_SYS_MSEC(2000)

        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT"))
        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0"))

        hio_sys_task_sleep(HIO_SYS_MSEC(2000));

        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSYSTEMMODE=0,1,0,0"))

        hio_sys_task_sleep(HIO_SYS_MSEC(2000));

        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CSCON=1"))
        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEREG=%d", 5))
        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CPSMS=1,,,\"00001000\",\"00000010\""))
        TRY(hio_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=1"))

        hio_sys_task_sleep(HIO_SYS_MSEC(3000));

        TRY(hio_lte_talk_cmd("AT+CIMI"))
        TRY(hio_lte_talk_rsp(&rsp, TIMEOUT_S))
        TRY(hio_lte_talk_ok(TIMEOUT_S))

        hio_sys_task_sleep(HIO_SYS_MINUTES(10));

        TRY(hio_lte_talk_cmd("AT#XPING=\"192.168.168.1\",50,5000"))
        TRY(hio_lte_talk_rsp(&rsp, TIMEOUT_S))
        TRY(hio_lte_talk_ok(TIMEOUT_S))

        TRY(hio_lte_talk_cmd("AT#XSLEEP=1"))
        TRY(hio_lte_talk_rsp(&rsp, HIO_SYS_MSEC(5000)))
        if (strcmp(rsp, "Ready") != 0) {
            goto error;
        }

#undef TRY

error:
        hio_sys_task_sleep(HIO_SYS_MSEC(30000));
    }
}

int
hio_net_lte_init(hio_net_lte_t *ctx, const hio_net_lte_cfg_t *cfg)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->cfg = cfg;

    hio_sys_sem_init(&ctx->sem, 0);
    hio_sys_task_init(&ctx->task, ctx->stack,
                      HIO_SYS_TASK_STACK_SIZEOF(ctx->stack), entry, ctx);

    return 0;
}

int
hio_net_lte_set_callback(hio_net_lte_t *ctx,
                         hio_net_lte_callback_t cb, void *param)
{
    ctx->cb = cb;
    ctx->param = param;

    return 0;
}

int
hio_net_lte_attach(hio_net_lte_t *ctx)
{
    return 0;
}

int
hio_net_lte_detach(hio_net_lte_t *ctx)
{
    return 0;
}

int
hio_net_lte_send(hio_net_lte_t *ctx, int port,
                 const void *buf, size_t len, hio_sys_timeout_t ttl)
{
    return 0;
}

int
hio_net_lte_recv(hio_net_lte_t *ctx, int *port,
                 void *buf, size_t size, size_t *len)
{
    return 0;
}
