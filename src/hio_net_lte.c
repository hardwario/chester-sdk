#include <hio_net_lte.h>
#include <hio_log.h>

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

    for (;;) {
        hio_log_debug("Alive");
        hio_sys_task_sleep(HIO_SYS_MSEC(1000));
    }
}

int
hio_net_lte_init(hio_net_lte_t *ctx, const hio_net_lte_cfg_t *cfg)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->cfg = cfg;

    hio_sys_sem_init(&ctx->sem, 0);
    hio_sys_task_init(&ctx->task, ctx->stack, HIO_SYS_TASK_STACK_SIZEOF(ctx->stack), entry, ctx);

    return 0;
}

int
hio_net_lte_set_callback(hio_net_lte_t *ctx, hio_net_lte_callback_t cb, void *param)
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
