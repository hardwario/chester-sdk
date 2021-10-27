#include <ctr_net_lte.h>

int ctr_net_lte_set_event_cb(ctr_net_lte_event_cb user_cb, void *user_data)
{
	return 0;
}

int ctr_net_lte_start(int *corr_id)
{
	return 0;
}

int ctr_net_lte_attach(int *corr_id)
{
	return 0;
}

int ctr_net_lte_detach(int *corr_id)
{
	return 0;
}

int ctr_net_lte_send(const struct ctr_net_lte_send_opts *opts, const void *buf, size_t len,
                     int *corr_id)
{
	return 0;
}

#if 0
#include <ctr_net_lte.h>
#include <ctr_bsp.h>
#include <ctr_lte_talk.h>
#include <ctr_lte_tok.h>
#include <ctr_lte_uart.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_net_lte, LOG_LEVEL_DBG);

#define SEND_MSGQ_MAX_ITEMS 16
#define RECV_MSGQ_MAX_ITEMS 16

#define SEND_MAX_LEN 512

#define TIMEOUT_S CTR_SYS_MSEC(5000)
#define TIMEOUT_L CTR_SYS_SECONDS(30)

typedef struct {
	int64_t ttl;
	uint8_t addr[4];
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
	STATE_DETACHED = 2,
} state_t;

typedef struct {
	const ctr_net_lte_cfg_t *cfg;
	ctr_net_lte_callback_t cb;
	void *param;
	ctr_sys_mut_t mut;
	ctr_sys_sem_t sem;
	ctr_sys_msgq_t send_msgq;
	char __aligned(4) send_msgq_mem[SEND_MSGQ_MAX_ITEMS * sizeof(send_item_t)];
	ctr_sys_msgq_t recv_msgq;
	char __aligned(4) recv_msgq_mem[RECV_MSGQ_MAX_ITEMS * sizeof(recv_item_t)];
	ctr_sys_task_t task;
	CTR_SYS_TASK_STACK_MEMBER(stack, 4096);
	state_t state_now;
	state_t state_req;
	bool registered;
	char buf[2 * SEND_MAX_LEN + 1];
} ctr_net_lte_t;

static ctr_net_lte_t inst;

static int sleep(void)
{
#if 1
	ctr_net_lte_t *ctx = &inst;

	int ret = 0;

	LOG_INF("Sleep initiated [%p]", ctx);

	/* TODO Unfortunately, we are not getting acknowledgement to this, but the */
	/* ticket has been raised */
	if (ctr_lte_talk_cmd("AT#XSLEEP=2") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		ret = -1;
	}

	/* TODO Why needed? */
	ctr_sys_task_sleep(CTR_SYS_SECONDS(2));
#endif

	if (ctr_lte_uart_disable()) {
		LOG_ERR("Call `ctr_lte_uart_disable` failed [%p]", ctx);
		ret = -2;
	}

	return ret;
}

static int wake_up(void)
{
#if 1
	ctr_net_lte_t *ctx = &inst;

	LOG_INF("Wake up initiated [%p]", ctx);

	if (ctr_lte_uart_enable() < 0) {
		LOG_ERR("Call `ctr_lte_uart_enable` failed [%p]", ctx);
		return -1;
	}

	if (ctr_bsp_set_lte_wkup(0) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_wkup` failed [%p]", ctx);
		return -2;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(10));

	if (ctr_bsp_set_lte_wkup(1) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_wkup` failed [%p]", ctx);
		return -3;
	}

	/* TODO Why needed? */
	ctr_sys_task_sleep(CTR_SYS_SECONDS(1));
#endif

	return 0;
}

static int attach_once(void)
{
	ctr_net_lte_t *ctx = &inst;

	if (ctr_bsp_set_lte_reset(0) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_reset` failed [%p]", ctx);
		return -1;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(10));

	if (ctr_bsp_set_lte_reset(1) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_reset` failed [%p]", ctx);
		return -2;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		return -3;
	}

	char *rsp;

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%HWVERSION") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -4;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%SHORTSWVER") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -5;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XPOFWARN=1,30") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -6;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSYSTEMMODE=0,1,0,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -7;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%REL14FEAT=1,1,1,1,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -8;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XDATAPRFL=0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -9;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSIM=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -10;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XNETTIME=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -11;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XTIME=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -12;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEREG=5") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -13;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CGEREP=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -14;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CSCON=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -15;
	}

	const char *timer_t3412 = "00111000"; /* T3412 Extended Timer */
	const char *timer_t3324 = "00000000"; /* T3324 Active Timer */

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CPSMS=1,,,\"%s\",\"%s\"", timer_t3412, timer_t3324) <
	    0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -16;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CNEC=24") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -17;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CMEE=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -18;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEPPI=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -19;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -20;
	}

	/* TODO Remove */
	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));

	if (ctr_lte_talk_cmd("AT+CIMI") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -21;
	}

	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -22;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -23;
	}

	int64_t end = ctr_sys_uptime_get() + CTR_SYS_MINUTES(5);

	for (;;) {
		int64_t now = ctr_sys_uptime_get();

		if (now >= end) {
			LOG_WRN("Attach timed out [%p]", ctx);
			return -24;
		}

		if (ctr_sys_sem_take(&ctx->sem, end - now) < 0) {
			continue;
		}

		ctr_sys_mut_acquire(&ctx->mut);
		bool registered = ctx->registered;
		ctr_sys_mut_release(&ctx->mut);

		if (registered) {
			break;
		}
	}

	if (ctr_lte_talk_cmd("AT#XSOCKET=1,2,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -25;
	}

	/* TODO Short timeout? */
	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -26;
	}

	if (strcmp(rsp, "#XSOCKET: 1,2,0,17") != 0) {
		LOG_ERR("Unexpected response [%p]", ctx);
		return -27;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -28;
	}

	if (sleep() < 0) {
		return -29;
	}

	/* TODO Disable UART */

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

static int attach(void)
{
	ctr_net_lte_t *ctx = &inst;

	LOG_INF("Attach started [%p]", ctx);

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->registered = false;
	int retries = ctx->cfg->attach_retries;
	int64_t pause = ctx->cfg->attach_pause;
	ctr_sys_mut_release(&ctx->mut);

	if (retries <= 0) {
		LOG_ERR("Parameter `retries` invalid [%p]", ctx);
		return -1;
	}

	do {
		if (attach_once() < 0) {
			goto error;
		}

		LOG_INF("Attach succeeded [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_net_lte_event_t event = { 0 };
			event.source = CTR_NET_LTE_EVENT_ATTACH_DONE;
			ctx->cb(&event, ctx->param);
		}

		return 0;

	error:
		LOG_WRN("Attach failed [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_net_lte_event_t event = { 0 };
			event.source = CTR_NET_LTE_EVENT_ATTACH_ERROR;
			ctx->cb(&event, ctx->param);
			if (event.opts.attach_error.stop) {
				LOG_INF("Attach cancelled [%p]", ctx);
				return -2;
			}
		}

		ctr_sys_task_sleep(pause);

	} while (--retries > 0);

	return -3;
}

static int detach(void)
{
	ctr_net_lte_t *ctx = &inst;

	LOG_INF("Detach started [%p]", ctx);

	int ret = 0;

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		ret = -1;
		goto error;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		ret = -2;
		goto error;
	}

	if (sleep() < 0) {
		LOG_ERR("Call `sleep` failed [%p]", ctx);
		ret = -3;
		goto error;
	}

	LOG_INF("Detach succeeded [%p]", ctx);

	if (ctx->cb != NULL) {
		ctr_net_lte_event_t event = { 0 };
		event.source = CTR_NET_LTE_EVENT_DETACH_DONE;
		ctx->cb(&event, ctx->param);
	}

	return 0;

error:
	LOG_WRN("Detach failed [%p]", ctx);

	if (ctx->cb != NULL) {
		ctr_net_lte_event_t event = { 0 };
		event.source = CTR_NET_LTE_EVENT_DETACH_ERROR;
		ctx->cb(&event, ctx->param);
	}

	return ret;
}

static state_t check_state(void)
{
	ctr_net_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	state_t state_now = ctx->state_now;
	state_t state_req = ctx->state_req;
	ctr_sys_mut_release(&ctx->mut);

	if (state_now != state_req) {
		switch (state_req) {
		case STATE_ATTACHED:
			if (attach() < 0) {
				LOG_WRN("Call `attach` failed [%p]", ctx);
			} else {
				state_now = STATE_ATTACHED;

				ctr_sys_mut_acquire(&ctx->mut);
				ctx->state_now = state_now;
				ctr_sys_mut_release(&ctx->mut);
			}
			break;
		case STATE_DETACHED:
			if (detach() < 0) {
				LOG_WRN("Call `detach` failed [%p]", ctx);
			} else {
				state_now = STATE_DETACHED;

				ctr_sys_mut_acquire(&ctx->mut);
				ctx->state_now = state_now;
				ctr_sys_mut_release(&ctx->mut);
			}
			break;
		default:
			LOG_ERR("Invalid state requested [%p]", ctx);
			break;
		}
	}

	return state_now;
}

static int hexify(char *dst, size_t size, const void *src, size_t len)
{
	if (size < len * 2 + 1) {
		return -ENOBUFS;
	}

	memset(dst, 0, size);

	for (uint8_t *p = (uint8_t *)src; len != 0; p++, len--) {
		*dst++ = (*p >> 4) >= 10 ? (*p >> 4) - 10 + 'A' : (*p >> 4) + '0';
		*dst++ = (*p & 15) >= 10 ? (*p & 15) - 10 + 'A' : (*p & 15) + '0';
	}

	return 0;
}

static int send_once(send_item_t *item)
{
	ctr_net_lte_t *ctx = &inst;

	if (hexify(ctx->buf, sizeof(ctx->buf), item->buf, item->len) < 0) {
		LOG_ERR("Call `hexify` failed [%p]", ctx);
		return -1;
	}

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		return -2;
	}

	char *rsp;

	if (ctr_lte_talk_cmd("AT#XSENDTO=\"%u.%u.%u.%u\",%u,0,\"%s\"", item->addr[0], item->addr[1],
	                     item->addr[2], item->addr[3], item->port, ctx->buf) < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -3;
	}

	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -4;
	}

	if ((rsp = ctr_lte_tok_pfx(rsp, "#XSENDTO: ")) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_pfx` failed [%p]", ctx);
		return -5;
	}

	bool def;
	long num;

	if ((rsp = ctr_lte_tok_num(rsp, &def, &num)) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_num` failed [%p]", ctx);
		return -6;
	}

	if (!def || num != item->len) {
		LOG_ERR("Number of sent bytes does not match [%p]", ctx);
		return -7;
	}

	if ((rsp = ctr_lte_tok_end(rsp)) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_end` failed [%p]", ctx);
		return -8;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -9;
	}

	if (sleep() < 0) {
		LOG_ERR("Call `sleep` failed [%p]", ctx);
		return -10;
	}

	return 0;
}

static int send(send_item_t *item)
{
	ctr_net_lte_t *ctx = &inst;

	LOG_INF("Send started [%p]", ctx);

	ctr_sys_mut_acquire(&ctx->mut);
	int retries = ctx->cfg->send_retries;
	int64_t pause = ctx->cfg->send_pause;
	ctr_sys_mut_release(&ctx->mut);

	if (retries <= 0) {
		LOG_ERR("Parameter `retries` invalid [%p]", ctx);
		return -1;
	}

	if (ctr_sys_uptime_get() > item->ttl) {
		LOG_WRN("Message TTL expired [%p]", ctx);
		return -2;
	}

	do {
		if (send_once(item) < 0) {
			goto error;
		}

		LOG_INF("Send succeeded [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_net_lte_event_t event = { 0 };
			event.source = CTR_NET_LTE_EVENT_SEND_DONE;
			event.info.send_done.buf = item->buf;
			event.info.send_done.len = item->len;
			ctx->cb(&event, ctx->param);
		}

		return 0;

	error:
		LOG_WRN("Send failed [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_net_lte_event_t event = { 0 };
			event.source = CTR_NET_LTE_EVENT_SEND_ERROR;
			ctx->cb(&event, ctx->param);
			if (event.opts.send_error.stop) {
				LOG_INF("Send cancelled [%p]", ctx);
				return -3;
			}
		}

		if (attach() < 0) {
			LOG_WRN("Call `attach` failed");
			return -4;
		}

		ctr_sys_task_sleep(pause);

	} while (--retries > 0);

	return -5;
}

static void check_send(void)
{
	ctr_net_lte_t *ctx = &inst;

	for (;;) {
		send_item_t item;

		if (ctr_sys_msgq_get(&ctx->send_msgq, &item, CTR_SYS_NO_WAIT) < 0) {
			break;
		}

		LOG_INF("Dequeued message to send (port %d, len %u) [%p]", item.port, item.len,
		        ctx);

		if (send(&item) < 0) {
			LOG_ERR("Call `send` failed [%p]", ctx);
		}
	}
}

static void entry(void *param)
{
	ctr_net_lte_t *ctx = param;

	if (ctr_bsp_set_rf_ant(CTR_BSP_RF_ANT_INT) < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_ant` failed [%p]", ctx);
		return;
	}

	if (ctr_bsp_set_rf_mux(CTR_BSP_RF_MUX_LTE) < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_mux` failed [%p]", ctx);
		return;
	}

	if (ctr_lte_uart_init() < 0) {
		LOG_ERR("Call `ctr_lte_uart_init` failed [%p]", ctx);
		return;
	}

	for (;;) {
		if (ctr_sys_sem_take(&ctx->sem, CTR_SYS_FOREVER) < 0) {
			LOG_ERR("Call `ctr_sys_sem_take` failed [%p]", ctx);
			return;
		}

		state_t state = check_state();

		if (state == STATE_ATTACHED) {
			check_send();
		}
	}
}

int ctr_net_lte_init(const ctr_net_lte_cfg_t *cfg)
{
	ctr_net_lte_t *ctx = &inst;

	memset(ctx, 0, sizeof(*ctx));

	ctx->cfg = cfg;

	ctr_sys_mut_init(&ctx->mut);
	ctr_sys_sem_init(&ctx->sem, 0);
	ctr_sys_msgq_init(&ctx->send_msgq, ctx->send_msgq_mem, sizeof(send_item_t),
	                  SEND_MSGQ_MAX_ITEMS);
	ctr_sys_msgq_init(&ctx->recv_msgq, ctx->recv_msgq_mem, sizeof(recv_item_t),
	                  RECV_MSGQ_MAX_ITEMS);
	ctr_sys_task_init(&ctx->task, "ctr_net_lte", ctx->stack,
	                  CTR_SYS_TASK_STACK_SIZEOF(ctx->stack), entry, ctx);

	return 0;
}

int ctr_net_lte_set_callback(ctr_net_lte_callback_t cb, void *param)
{
	ctr_net_lte_t *ctx = &inst;

	ctx->cb = cb;
	ctx->param = param;

	return 0;
}

int ctr_net_lte_attach(void)
{
	ctr_net_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->state_req = STATE_ATTACHED;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_net_lte_detach(void)
{
	ctr_net_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->state_req = STATE_DETACHED;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_net_lte_send(const ctr_net_send_opts_t *opts, const void *buf, size_t len)
{
	ctr_net_lte_t *ctx = &inst;

	send_item_t item = {
		.ttl = opts->ttl,
		.addr = { opts->addr[0], opts->addr[1], opts->addr[2], opts->addr[3] },
		.port = opts->port,
		.buf = buf,
		.len = len,
	};

	if (ctr_sys_msgq_put(&ctx->send_msgq, &item, CTR_SYS_NO_WAIT) < 0) {
		LOG_ERR("Call `ctr_sys_msgq_put` failed [%p]", ctx);
		return -1;
	}

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_net_lte_recv(ctr_net_recv_info_t *info, void *buf, size_t size)
{
	return 0;
}

void ctr_net_lte_set_reg(bool state)
{
	ctr_net_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->registered = state;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);
}

/*

test start
test lte reset
test lte wakeup
test lte cmd AT%XSYSTEMMODE=0,1,0,0
test lte cmd AT%REL14FEAT=1,1,1,1,0
test lte cmd AT%XDATAPRFL=0
test lte cmd AT%XSIM=1
test lte cmd AT%XNETTIME=1
test lte cmd AT%XTIME=1
test lte cmd AT+CEREG=5
test lte cmd AT+CGEREP=1
test lte cmd AT+CSCON=1
test lte cmd 'AT+CPSMS=1,,,"00111000","00000000"'
test lte cmd AT+CNEC=24
test lte cmd AT+CMEE=1
test lte cmd AT+CEPPI=1
test lte cmd AT+CFUN=1



*/
#endif
