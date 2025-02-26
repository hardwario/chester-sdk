/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_state.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define WAKEUP_TIMEOUT         K_SECONDS(5)
#define SIMDETECTED_TIMEOUT    K_SECONDS(10)
#define MDMEV_RESET_LOOP_DELAY K_MINUTES(32)
#define SEND_CSCON_1_TIMEOUT   K_SECONDS(30)
#define CONEVAL_TIMEOUT        K_SECONDS(30)

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

enum ctr_lte_v2_state {
	CTR_LTE_V2_STATE_DISABLED = 0,
	CTR_LTE_V2_STATE_ERROR,
	CTR_LTE_V2_STATE_BOOT,
	CTR_LTE_V2_STATE_PREPARE,
	CTR_LTE_V2_STATE_ATTACH,
	CTR_LTE_V2_STATE_RETRY_DELAY,
	CTR_LTE_V2_STATE_RESET_LOOP,
	CTR_LTE_V2_STATE_OPEN_SOCKET,
	CTR_LTE_V2_STATE_READY,
	CTR_LTE_V2_STATE_SLEEP,
	CTR_LTE_V2_STATE_WAKEUP,
	CTR_LTE_V2_STATE_SEND,
	CTR_LTE_V2_STATE_RECEIVE,
	CTR_LTE_V2_STATE_CONEVAL,
	CTR_LTE_V2_STATE_GNSS,
};

struct ctr_lte_fsm_state {
	enum ctr_lte_v2_state state;
	int (*on_enter)(void);
	int (*on_leave)(void);
	int (*event_handler)(enum ctr_lte_v2_event event);
};

struct attach_timeout {
	k_timeout_t retry_delay;
	k_timeout_t attach_timeout;
};

int m_attach_retry_count = 0;
enum ctr_lte_v2_state m_state;
struct k_work_delayable m_timeout_work;
struct k_work m_event_dispatch_work;
uint8_t m_event_buf[8];
bool m_cscon = false;
struct ring_buf m_event_rb;
K_MUTEX_DEFINE(m_event_rb_lock);

static K_EVENT_DEFINE(m_states_event);
#define CONNECTED_BIT BIT(0)
#define SEND_RECV_BIT BIT(1)

K_MUTEX_DEFINE(m_send_recv_lock);
struct ctr_lte_v2_send_recv_param *m_send_recv_param = NULL;

struct ctr_lte_v2_metrics m_metrics;
K_MUTEX_DEFINE(m_metrics_lock);

atomic_t m_gnss_enable = ATOMIC_INIT(0);
atomic_ptr_t m_gnss_cb = ATOMIC_PTR_INIT(NULL);
void *m_gnss_user_data = NULL;
struct k_work m_gnss_work;

static struct ctr_lte_fsm_state *get_fsm_state(enum ctr_lte_v2_state state);

struct attach_timeout get_attach_timeout(int count)
{
	switch (count) {
	case 0:
		return (struct attach_timeout){K_NO_WAIT, K_MINUTES(5)};
	case 1:
		return (struct attach_timeout){K_NO_WAIT, K_MINUTES(5)};
	case 2:
		return (struct attach_timeout){K_NO_WAIT, K_MINUTES(50)};
	case 3:
		return (struct attach_timeout){K_HOURS(1), K_MINUTES(5)};
	case 4:
		return (struct attach_timeout){K_MINUTES(5), K_MINUTES(45)};
	case 5:
		return (struct attach_timeout){K_HOURS(6), K_MINUTES(5)};
	case 6:
		return (struct attach_timeout){K_MINUTES(5), K_MINUTES(45)};
	case 7:
		return (struct attach_timeout){K_HOURS(24), K_MINUTES(5)};
	case 8:
		return (struct attach_timeout){K_MINUTES(5), K_MINUTES(45)};
	default:
		if (count % 2 != 0) { /*  9, 11 ... */
			return (struct attach_timeout){K_HOURS(168), K_MINUTES(5)};
		} else { /* 10, 12 ... */
			return (struct attach_timeout){K_MINUTES(5), K_MINUTES(45)};
		}
	}
}

const char *ctr_lte_v2_state_str(enum ctr_lte_v2_state state)
{
	switch (state) {
	case CTR_LTE_V2_STATE_DISABLED:
		return "disabled";
	case CTR_LTE_V2_STATE_ERROR:
		return "error";
	case CTR_LTE_V2_STATE_BOOT:
		return "boot";
	case CTR_LTE_V2_STATE_PREPARE:
		return "prepare";
	case CTR_LTE_V2_STATE_RESET_LOOP:
		return "reset_loop";
	case CTR_LTE_V2_STATE_RETRY_DELAY:
		return "retry_delay";
	case CTR_LTE_V2_STATE_ATTACH:
		return "attach";
	case CTR_LTE_V2_STATE_OPEN_SOCKET:
		return "open_socket";
	case CTR_LTE_V2_STATE_READY:
		return "ready";
	case CTR_LTE_V2_STATE_SLEEP:
		return "sleep";
	case CTR_LTE_V2_STATE_WAKEUP:
		return "wakeup";
	case CTR_LTE_V2_STATE_SEND:
		return "send";
	case CTR_LTE_V2_STATE_RECEIVE:
		return "receive";
	case CTR_LTE_V2_STATE_CONEVAL:
		return "coneval";
	case CTR_LTE_V2_STATE_GNSS:
		return "gnss";
	}
	return "unknown";
}

const char *ctr_lte_v2_event_str(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_ERROR:
		return "error";
	case CTR_LTE_V2_EVENT_TIMEOUT:
		return "timeout";
	case CTR_LTE_V2_EVENT_ENABLE:
		return "enable";
	case CTR_LTE_V2_EVENT_READY:
		return "ready";
	case CTR_LTE_V2_EVENT_SIMDETECTED:
		return "simdetected";
	case CTR_LTE_V2_EVENT_REGISTERED:
		return "registered";
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		return "deregistered";
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		return "resetloop";
	case CTR_LTE_V2_EVENT_SOCKET_OPENED:
		return "socket_opened";
	case CTR_LTE_V2_EVENT_XMODMSLEEEP:
		return "xmodmsleep";
	case CTR_LTE_V2_EVENT_CSCON_0:
		return "cscon_0";
	case CTR_LTE_V2_EVENT_CSCON_1:
		return "cscon_1";
	case CTR_LTE_V2_EVENT_XTIME:
		return "xtime";
	case CTR_LTE_V2_EVENT_SEND:
		return "send";
	case CTR_LTE_V2_EVENT_RECV:
		return "recv";
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		return "xgps_enable";
	case CTR_LTE_V2_EVENT_XGPS_DISABLE:
		return "xgps_disable";
	case CTR_LTE_V2_EVENT_XGPS:
		return "xgps";
	}
	return "unknown";
}

const char *ctr_lte_v2_coneval_result_str(int result)
{
	switch (result) {
	case 0:
		return "Connection pre-evaluation successful";
	case 1:
		return "Evaluation failed, no cell available";
	case 2:
		return "Evaluation failed, UICC not available";
	case 3:
		return "Evaluation failed, only barred cells available";
	case 4:
		return "Evaluation failed, busy";
	case 5:
		return "Evaluation failed, aborted because of higher priority operation";
	case 6:
		return "Evaluation failed, not registered";
	case 7:
		return "Evaluation failed, unspecified";
	default:
		return "Evaluation failed, unknown result";
	}
}

const char *ctr_lte_v2_get_state(void)
{
	return ctr_lte_v2_state_str(m_state);
}

int ctr_lte_v2_get_timeout_remaining(void)
{
	k_ticks_t ticks = k_work_delayable_remaining_get(&m_timeout_work);
	return k_ticks_to_ms_ceil32(ticks);
}

static void start_timer(k_timeout_t timeout)
{
	k_work_schedule_for_queue(&m_work_q, &m_timeout_work, timeout);
}

static void stop_timer(void)
{
	k_work_cancel_delayable(&m_timeout_work);
}

static void delegate_event(enum ctr_lte_v2_event event)
{
	k_mutex_lock(&m_event_rb_lock, K_FOREVER);
	int ret = ring_buf_put(&m_event_rb, (uint8_t *)&event, 1);
	k_mutex_unlock(&m_event_rb_lock);

	if (ret < 0) {
		LOG_WRN("Failed to put event in ring buffer");
		return;
	}

	ret = k_work_submit_to_queue(&m_work_q, &m_event_dispatch_work);
	if (ret < 0) {
		LOG_WRN("Failed to submit work to queue");
	}
}

static void event_handler(enum ctr_lte_v2_event event)
{
	LOG_DBG("event: %s, state: %s", ctr_lte_v2_event_str(event), ctr_lte_v2_state_str(m_state));

	struct ctr_lte_fsm_state *fsm_state = get_fsm_state(m_state);
	if (fsm_state && fsm_state->event_handler) {
		int ret = fsm_state->event_handler(event);
		if (ret < 0) {
			LOG_WRN("failed to handle event, error: %i", ret);
			if (event != CTR_LTE_V2_EVENT_ERROR) {
				delegate_event(CTR_LTE_V2_EVENT_ERROR);
			}
		}
	}
}

static void enter_state(enum ctr_lte_v2_state state)
{
	int ret;

	LOG_DBG("leaving state: %s", ctr_lte_v2_state_str(m_state));
	struct ctr_lte_fsm_state *fsm_state = get_fsm_state(m_state);
	if (fsm_state && fsm_state->on_leave) {
		ret = fsm_state->on_leave();
		if (ret < 0) {
			LOG_WRN("failed to leave state, error: %i", ret);
			if (state != CTR_LTE_V2_STATE_ERROR) {
				delegate_event(CTR_LTE_V2_EVENT_ERROR);
			}
			return;
		}
	}

	m_state = state;
	LOG_DBG("entering to state: %s", ctr_lte_v2_state_str(state));
	fsm_state = get_fsm_state(state);
	if (fsm_state && fsm_state->on_enter) {
		ret = fsm_state->on_enter();
		if (ret < 0) {
			LOG_WRN("failed to enter state error: %i", ret);
			if (state != CTR_LTE_V2_STATE_ERROR) {
				delegate_event(CTR_LTE_V2_EVENT_ERROR);
			}
			return;
		}
	}
}

static void timeout_work_handler(struct k_work *item)
{
	delegate_event(CTR_LTE_V2_EVENT_TIMEOUT);
}

static void event_dispatch_work_handler(struct k_work *item)
{
	uint8_t events[sizeof(m_event_buf)];
	uint8_t events_cnt;

	k_mutex_lock(&m_event_rb_lock, K_FOREVER);

	events_cnt = (uint8_t)ring_buf_get(&m_event_rb, events, ARRAY_SIZE(events));

	k_mutex_unlock(&m_event_rb_lock);

	for (uint8_t i = 0; i < events_cnt; i++) {
		event_handler((enum ctr_lte_v2_event)events[i]);
	}
}

int ctr_lte_v2_enable(void)
{
	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("LTE Test mode enabled");
		return -ENOTSUP;
	}
	delegate_event(CTR_LTE_V2_EVENT_ENABLE);
	return 0;
}

int ctr_lte_v2_is_attached(bool *attached)
{
	*attached = k_event_test(&m_states_event, CONNECTED_BIT) ? true : false;
	return 0;
}

int ctr_lte_v2_wait_for_connected(k_timeout_t timeout)
{
	if (k_event_wait(&m_states_event, CONNECTED_BIT, false, timeout)) {
		return 0;
	}
	return -ETIMEDOUT;
}

int ctr_lte_v2_get_imei(uint64_t *imei)
{
	return ctr_lte_v2_state_get_imei(imei);
}

int ctr_lte_v2_get_imsi(uint64_t *imsi)
{
	return ctr_lte_v2_state_get_imsi(imsi);
}

int ctr_lte_v2_get_iccid(char **iccid)
{
	return ctr_lte_v2_state_get_iccid(iccid);
}

int ctr_lte_v2_get_modem_fw_version(char **version)
{
	return ctr_lte_v2_state_get_modem_fw_version(version);
}

int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param)
{
	LOG_INF("GET LOCK");

	k_timepoint_t end = sys_timepoint_calc(param->timeout);

	k_mutex_lock(&m_send_recv_lock, sys_timepoint_timeout(end));

	LOG_INF("LOCKED");

	m_send_recv_param = (struct ctr_lte_v2_send_recv_param *)param;

	k_event_clear(&m_states_event, SEND_RECV_BIT);

	delegate_event(CTR_LTE_V2_EVENT_SEND);

	LOG_INF("WAITING for send/recv");

	k_event_wait(&m_states_event, SEND_RECV_BIT, false, sys_timepoint_timeout(end));

	if (sys_timepoint_expired(end)) {
		k_mutex_unlock(&m_send_recv_lock);
		delegate_event(CTR_LTE_V2_EVENT_TIMEOUT);
		return -ETIMEDOUT;
	}

	k_mutex_unlock(&m_send_recv_lock);

	LOG_INF("UNLOCK");

	return 0;
}

int ctr_lte_v2_get_conn_param(struct ctr_lte_v2_conn_param *param)
{
	return ctr_lte_v2_state_get_conn_param(param);
}

int ctr_lte_v2_get_cereg_param(struct ctr_lte_v2_cereg_param *param)
{
	return ctr_lte_v2_state_get_cereg_param(param);
}

int ctr_lte_v2_get_metrics(struct ctr_lte_v2_metrics *metrics)
{
	if (!metrics) {
		return -EINVAL;
	}
	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	memcpy(metrics, &m_metrics, sizeof(struct ctr_lte_v2_metrics));
	k_mutex_unlock(&m_metrics_lock);
	return 0;
}

#if defined(CONFIG_CTR_LTE_V2_GNSS)
static void gnss_work_handler(struct k_work *item)
{
	ctr_lte_v2_gnss_cb cb = (ctr_lte_v2_gnss_cb)atomic_ptr_get(&m_gnss_cb);
	struct ctr_lte_v2_gnss_update update;
	ctr_lte_v2_state_get_gnss_update(&update);
	if (cb) {
		cb(&update, m_gnss_user_data);
	}
}

int ctr_lte_v2_gnss_set_enable(bool enable)
{
	atomic_set(&m_gnss_enable, enable ? 1 : 0);
	if (enable) {
		delegate_event(CTR_LTE_V2_EVENT_XGPS_ENABLE);
	} else {
		delegate_event(CTR_LTE_V2_EVENT_XGPS_DISABLE);
	}
	return 0;
}

int ctr_lte_v2_gnss_get_enable(bool *enable)
{
	if (!enable) {
		return -EINVAL;
	}
	*enable = atomic_get(&m_gnss_enable) ? true : false;
	return 0;
}

int ctr_lte_v2_gnss_set_handler(ctr_lte_v2_gnss_cb callback, void *user_data)
{
	if (!callback) {
		return -EINVAL;
	}
	atomic_ptr_set(&m_gnss_cb, (atomic_ptr_val_t)callback);
	m_gnss_user_data = user_data;
	return 0;
}
#endif

static int on_enter_disabled(void)
{
	int ret;
	ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
	}

	ret = ctr_lte_v2_flow_rfmux_release();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_rfmux_release` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int disabled_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_ENABLE:
		enter_state(CTR_LTE_V2_STATE_BOOT);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_disabled(void)
{
	int ret = ctr_lte_v2_flow_rfmux_acquire();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_rfmux_acquire` failed: %d", ret);
		return ret;
	}
	return 0;
}

static int on_enter_error(void)
{
	k_event_clear(&m_states_event, CONNECTED_BIT);

	int ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
	}

	start_timer(K_SECONDS(10)); /* TODO: retry timeout (progressive) */

	return 0;
}

static int error_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_BOOT);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_boot(void)
{
	int ret;

	ret = ctr_lte_v2_flow_reset();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_reset` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_flow_enable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_enable` failed: %d", ret);
		return ret;
	}

	start_timer(WAKEUP_TIMEOUT);

	return 0;
}

static int boot_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_READY:
		enter_state(CTR_LTE_V2_STATE_PREPARE);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_boot(void)
{
	stop_timer();
	return 0;
}

static int on_enter_prepare(void)
{
	int ret = ctr_lte_v2_flow_prepare();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_prepare` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_flow_cfun(1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_cfun` failed: %d", ret);
		return ret;
	}

	start_timer(SIMDETECTED_TIMEOUT);

	return 0;
}

static int prepare_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SIMDETECTED:
		stop_timer();
		int ret = ctr_lte_v2_flow_sim_info();
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_sim_info` failed: %d", ret);
			return ret;
		}
		ret = ctr_lte_v2_flow_sim_fplmn();
		if (ret) {
			if (ret == -EAGAIN) {
				break;
			}
			LOG_ERR("Call `ctr_lte_v2_flow_sim_fplmn` failed: %d", ret);
			return ret;
		}
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		enter_state(CTR_LTE_V2_STATE_RESET_LOOP);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_prepare(void)
{
	stop_timer();
	return 0;
}

static int on_enter_reset_loop(void)
{
	int ret = ctr_lte_v2_flow_cfun(4);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_cfun` 4 failed: %d", ret);
		return ret;
	}

	k_sleep(K_SECONDS(5));

	ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
		return ret;
	}

	start_timer(MDMEV_RESET_LOOP_DELAY);

	return 0;
}

static int reset_loop_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_reset_loop(void)
{
	int ret = ctr_lte_v2_flow_enable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_enable` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_flow_cfun(0);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	k_sleep(K_SECONDS(5));

	ret = ctr_lte_v2_flow_cfun(1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int on_enter_retry_delay(void)
{
	int ret = ctr_lte_v2_flow_cfun(4);
	if (ret) {
		LOG_WRN("Call `ctr_lte_v2_flow_cfun` failed: %d", ret);
	}

	k_sleep(K_SECONDS(5));

	ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
		return ret;
	}

	struct attach_timeout timeout = get_attach_timeout(m_attach_retry_count);

	LOG_INF("Waiting %lld minutes before attach retry",
		k_ticks_to_ms_floor64(timeout.retry_delay.ticks) / MSEC_PER_SEC / 60);

	start_timer(timeout.retry_delay);
	return 0;
}

static int retry_delay_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_BOOT);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_attach(void)
{
	k_event_clear(&m_states_event, CONNECTED_BIT);

	struct attach_timeout timeout = get_attach_timeout(m_attach_retry_count++);

	LOG_INF("Try to attach with timeout %lld s",
		k_ticks_to_ms_floor64(timeout.attach_timeout.ticks) / MSEC_PER_SEC);

	start_timer(timeout.attach_timeout);

	return 0;
}

static int attach_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_REGISTERED:
		m_attach_retry_count = 0;
		enter_state(CTR_LTE_V2_STATE_OPEN_SOCKET);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		m_cscon = true;
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		break;
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		m_attach_retry_count = 0;
		enter_state(CTR_LTE_V2_STATE_RESET_LOOP);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_RETRY_DELAY);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_attach(void)
{
	stop_timer();
	return 0;
}

static int on_enter_open_socket(void)
{
	int ret = ctr_lte_v2_flow_open_socket();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_open_socket` failed: %d", ret);
		return ret;
	}

	delegate_event(CTR_LTE_V2_EVENT_SOCKET_OPENED);

	return 0;
}

static int open_socket_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SOCKET_OPENED:
		k_event_post(&m_states_event, CONNECTED_BIT);
		enter_state(CTR_LTE_V2_STATE_CONEVAL);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		m_cscon = true;
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_ready(void)
{
	if (m_send_recv_param) {
		delegate_event(CTR_LTE_V2_EVENT_SEND);
	}
#if defined(CONFIG_CTR_LTE_V2_GNSS)
	else if (atomic_get(&m_gnss_enable)) { /* Only one event delegate at a time */
		delegate_event(CTR_LTE_V2_EVENT_XGPS_ENABLE);
	}
#endif
	return 0;
}

static int ready_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SEND:
		int ret = ctr_lte_v2_flow_check();
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_check` failed: %d", ret);
			if (ret == -ENOTCONN) {
				delegate_event(CTR_LTE_V2_EVENT_DEREGISTERED);
				return 0;
			}
			return ret;
		}
		enter_state(CTR_LTE_V2_STATE_SEND);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		m_cscon = true;
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		break;
	case CTR_LTE_V2_EVENT_XMODMSLEEEP:
#if defined(CONFIG_CTR_LTE_V2_GNSS)
		__fallthrough;
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		if (atomic_get(&m_gnss_enable)) {
			enter_state(CTR_LTE_V2_STATE_GNSS);
			break;
		}
#endif
		enter_state(CTR_LTE_V2_STATE_SLEEP);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_sleep(void)
{
	int ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
		return ret;
	}

	if (m_send_recv_param) {
		delegate_event(CTR_LTE_V2_EVENT_SEND);
	}

	return 0;
}

static int sleep_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SEND:
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		enter_state(CTR_LTE_V2_STATE_WAKEUP);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_wakeup(void)
{
	int ret = ctr_lte_v2_flow_enable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_enable` failed: %d", ret);
		return ret;
	}

	start_timer(WAKEUP_TIMEOUT);

	return 0;
}

static int wakeup_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_READY:
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_wakeup(void)
{
	stop_timer();
	return 0;
}

static int on_enter_send(void)
{
	int ret;

	if (!m_send_recv_param) {
		delegate_event(CTR_LTE_V2_EVENT_READY);
		return 0;
	}

	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	m_metrics.uplink_count++;
	m_metrics.uplink_bytes += m_send_recv_param->send_len;
	ret = ctr_rtc_get_ts(&m_metrics.uplink_last_ts);
	if (ret) {
		LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
	}
	k_mutex_unlock(&m_metrics_lock);

	ret = ctr_lte_v2_flow_send(m_send_recv_param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_send` failed: %d", ret);

		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.uplink_errors++;
		k_mutex_unlock(&m_metrics_lock);

		return ret;
	}

	start_timer(SEND_CSCON_1_TIMEOUT);

	if (m_cscon) {
		delegate_event(CTR_LTE_V2_EVENT_SEND);
	}

	return 0;
}

static int send_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		m_cscon = true; /* Continue to send */
	case CTR_LTE_V2_EVENT_SEND:
		stop_timer();
		if (m_send_recv_param) {
			if (m_send_recv_param->recv_buf) {
				enter_state(CTR_LTE_V2_STATE_RECEIVE);
			} else {
				m_send_recv_param = NULL;
				k_event_post(&m_states_event, SEND_RECV_BIT);
				enter_state(CTR_LTE_V2_STATE_CONEVAL);
			}
		} else {
			enter_state(CTR_LTE_V2_STATE_READY);
		}
		break;
	case CTR_LTE_V2_EVENT_READY:
	case CTR_LTE_V2_EVENT_TIMEOUT:
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.uplink_errors++;
		k_mutex_unlock(&m_metrics_lock);
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_send(void)
{
	stop_timer();
	return 0;
}

static int on_enter_receive(void)
{
	int ret;
	if (!m_send_recv_param) {
		delegate_event(CTR_LTE_V2_EVENT_READY);
		return 0;
	}

	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	m_metrics.downlink_count++;
	ret = ctr_rtc_get_ts(&m_metrics.downlink_last_ts);
	if (ret) {
		LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
	}
	k_mutex_unlock(&m_metrics_lock);

	ret = ctr_lte_v2_flow_recv(m_send_recv_param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_send` failed: %d", ret);
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.downlink_errors++;
		k_mutex_unlock(&m_metrics_lock);
		return ret;
	}

	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	m_metrics.downlink_bytes += *m_send_recv_param->recv_len;
	k_mutex_unlock(&m_metrics_lock);

	if (!m_send_recv_param->rai) {
		delegate_event(CTR_LTE_V2_EVENT_RECV);
	}

	m_send_recv_param = NULL;
	k_event_post(&m_states_event, SEND_RECV_BIT);

	return 0;
}

static int receive_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_RECV:
	case CTR_LTE_V2_EVENT_READY:
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		enter_state(CTR_LTE_V2_STATE_CONEVAL);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_enter_coneval(void)
{
	int ret = ctr_lte_v2_flow_coneval();
	if (ret) {
		LOG_WRN("Call `ctr_lte_v2_flow_coneval` failed: %d", ret);
		return ret;
	}

	delegate_event(CTR_LTE_V2_EVENT_READY);

	return 0;
}

static int coneval_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_READY:
	case CTR_LTE_V2_EVENT_TIMEOUT:
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		m_cscon = false;
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		m_cscon = true;
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		enter_state(CTR_LTE_V2_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		enter_state(CTR_LTE_V2_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

#if defined(CONFIG_CTR_LTE_V2_GNSS)
static int on_enter_gnss(void)
{
	int ret;

	ret = ctr_lte_v2_flow_cmd_without_response("AT#XGPS=1,0,1");
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_cmd_without_response` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int gnss_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SEND:
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_XGPS:
		k_work_submit_to_queue(&m_work_q, &m_gnss_work);
		break;
	case CTR_LTE_V2_EVENT_XGPS_DISABLE:
		enter_state(CTR_LTE_V2_STATE_READY);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_gnss(void)
{
	int ret;
	ret = ctr_lte_v2_flow_cmd_without_response("AT#XGPS=0");
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_cmd_without_response AT#XGPS=0` failed: %d", ret);
		return ret;
	}
	return 0;
}
#endif

/* clang-format off */
static struct ctr_lte_fsm_state m_fsm_states[] = {
	{CTR_LTE_V2_STATE_DISABLED, on_enter_disabled, on_leave_disabled, disabled_event_handler},
	{CTR_LTE_V2_STATE_ERROR, on_enter_error, NULL, error_event_handler},
	{CTR_LTE_V2_STATE_BOOT, on_enter_boot, on_leave_boot, boot_event_handler},
	{CTR_LTE_V2_STATE_PREPARE, on_enter_prepare, on_leave_prepare, prepare_event_handler},
	{CTR_LTE_V2_STATE_RESET_LOOP, on_enter_reset_loop, on_leave_reset_loop, reset_loop_event_handler},
	{CTR_LTE_V2_STATE_RETRY_DELAY, on_enter_retry_delay, NULL, retry_delay_event_handler},
	{CTR_LTE_V2_STATE_ATTACH, on_enter_attach, on_leave_attach, attach_event_handler},
	{CTR_LTE_V2_STATE_OPEN_SOCKET, on_enter_open_socket, NULL, open_socket_event_handler},
	{CTR_LTE_V2_STATE_READY, on_enter_ready, NULL, ready_event_handler},
	{CTR_LTE_V2_STATE_SLEEP, on_enter_sleep, NULL, sleep_event_handler},
	{CTR_LTE_V2_STATE_WAKEUP, on_enter_wakeup, on_leave_wakeup, wakeup_event_handler},
	{CTR_LTE_V2_STATE_SEND, on_enter_send, on_leave_send, send_event_handler},
	{CTR_LTE_V2_STATE_RECEIVE, on_enter_receive, NULL, receive_event_handler},
	{CTR_LTE_V2_STATE_CONEVAL, on_enter_coneval, NULL, coneval_event_handler},
	#if defined(CONFIG_CTR_LTE_V2_GNSS)
	{CTR_LTE_V2_STATE_GNSS, on_enter_gnss, on_leave_gnss, gnss_event_handler},
	#endif
};
/* clang-format on */

struct ctr_lte_fsm_state *get_fsm_state(enum ctr_lte_v2_state state)
{
	for (size_t i = 0; i < ARRAY_SIZE(m_fsm_states); i++) {
		if (m_fsm_states[i].state == state) {
			return &m_fsm_states[i];
		}
	}

	LOG_ERR("Unknown state %s %d", ctr_lte_v2_state_str(state), state);

	return NULL;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	ret = ctr_lte_v2_config_init();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_config_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_flow_init(delegate_event);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_init` failed: %d", ret);
		return ret;
	}

	m_state = CTR_LTE_V2_STATE_DISABLED;

	k_work_queue_init(&m_work_q);

	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	k_work_init_delayable(&m_timeout_work, timeout_work_handler);
	k_work_init(&m_event_dispatch_work, event_dispatch_work_handler);
	ring_buf_init(&m_event_rb, sizeof(m_event_buf), m_event_buf);
#if defined(CONFIG_CTR_LTE_V2_GNSS)
	k_work_init(&m_gnss_work, gnss_work_handler);
#endif
	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_LTE_V2_INIT_PRIORITY);
