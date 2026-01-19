/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_state.h"
#include "ctr_lte_v2_str.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/time_units.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define WAKEUP_TIMEOUT         K_SECONDS(10)
#define SIMDETECTED_TIMEOUT    K_SECONDS(10)
#define MDMEV_RESET_LOOP_DELAY K_MINUTES(32)
#define SEND_CSCON_1_TIMEOUT   K_SECONDS(30)
#define CONEVAL_TIMEOUT        K_SECONDS(30)
#define FSM_STOP_TIMEOUT       K_SECONDS(30)

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

enum fsm_state {
	FSM_STATE_DISABLED = 0,
	FSM_STATE_ERROR,
	FSM_STATE_BOOT,
	FSM_STATE_PREPARE,
	FSM_STATE_ATTACH,
	FSM_STATE_RETRY_DELAY,
	FSM_STATE_RESET_LOOP,
	FSM_STATE_OPEN_SOCKET,
	FSM_STATE_READY,
	FSM_STATE_SLEEP,
	FSM_STATE_WAKEUP,
	FSM_STATE_SEND,
	FSM_STATE_RECEIVE,
	FSM_STATE_CONEVAL,
	FSM_STATE_GNSS,
};

struct fsm_state_desc {
	enum fsm_state state;
	int (*on_enter)(void);
	int (*on_leave)(void);
	int (*event_handler)(enum ctr_lte_v2_event event);
};

struct attach_timeout {
	k_timeout_t retry_delay;
	k_timeout_t attach_timeout;
};

int m_attach_retry_count = 0;
enum fsm_state m_state;
struct k_work_delayable m_timeout_work;
struct k_work m_event_dispatch_work;
uint8_t m_event_buf[8];
struct ring_buf m_event_rb;
K_MUTEX_DEFINE(m_event_rb_lock);

#define FLAG_CSCON        BIT(0)
#define FLAG_GNSS_ENABLE  BIT(1)
#define FLAG_CFUN4        BIT(2)
#define FLAG_SEND_PENDING BIT(3)
#define FLAG_RECV_PENDING BIT(4)
atomic_t m_flag = ATOMIC_INIT(0);

#define CONNECTED_BIT      BIT(0)
#define SEND_RECV_DONE_BIT BIT(1)
#define SEND_RECV_STOP_BIT BIT(2)
static K_EVENT_DEFINE(m_states_event);

K_MUTEX_DEFINE(m_send_recv_lock);
struct ctr_lte_v2_send_recv_param *m_send_recv_param = NULL;
static int m_send_recv_result = 0;

K_MUTEX_DEFINE(m_metrics_lock);
struct ctr_lte_v2_metrics m_metrics;
static uint32_t m_start = 0;
static uint32_t m_start_cscon1 = 0;

struct ctr_lte_v2_cereg_param m_cereg_param = {0};

#define ON_ERROR_MAX_FLOW_CHECK_RETRIES 3
static struct {
	enum fsm_state prev_state;
	int flow_check_failures;
	enum fsm_state on_timeout_state;
	uint32_t timeout_s;
} m_error_ctx = {0};

#if defined(CONFIG_CTR_LTE_V2_GNSS)
atomic_ptr_t m_gnss_cb = ATOMIC_PTR_INIT(NULL);
void *m_gnss_user_data = NULL;
struct k_work m_gnss_work;
#endif

static struct fsm_state_desc *get_fsm_state(enum fsm_state state);

static struct ctr_lte_v2_attach_timeout get_attach_timeout(int attempt)
{
	switch (g_ctr_lte_v2_config.attach_policy) {
	case CTR_LTE_V2_ATTACH_POLICY_AGGRESSIVE:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_NO_WAIT);
	case CTR_LTE_V2_ATTACH_POLICY_PERIODIC_2H:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_HOURS(1));
	case CTR_LTE_V2_ATTACH_POLICY_PERIODIC_6H:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_HOURS(5));
	case CTR_LTE_V2_ATTACH_POLICY_PERIODIC_12H:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_HOURS(11));
	case CTR_LTE_V2_ATTACH_POLICY_PERIODIC_1D:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_HOURS(23));
	case CTR_LTE_V2_ATTACH_POLICY_PROGRESSIVE:
		return ctr_lte_v2_flow_attach_policy_progressive(attempt);
	default:
		return ctr_lte_v2_flow_attach_policy_periodic(attempt, K_HOURS(1));
	}
}

int ctr_lte_v2_get_curr_attach_info(int *attempt, int *attach_timeout_sec, int *retry_delay_sec,
				    int *remaining_sec)
{
	struct ctr_lte_v2_attach_timeout timeout = get_attach_timeout(m_attach_retry_count);
	if (attempt) {
		*attempt = m_attach_retry_count + 1;
	}
	if (attach_timeout_sec) {
		*attach_timeout_sec = k_ticks_to_sec_floor32(timeout.attach_timeout.ticks);
	}
	if (retry_delay_sec) {
		*retry_delay_sec = k_ticks_to_sec_floor32(timeout.retry_delay.ticks);
	}
	if (remaining_sec) {
		k_ticks_t ticks = k_work_delayable_remaining_get(&m_timeout_work);
		*remaining_sec = k_ticks_to_sec_floor32(ticks);
	}
	return 0;
}

const char *fsm_state_str(enum fsm_state state)
{
	switch (state) {
	case FSM_STATE_DISABLED:
		return "disabled";
	case FSM_STATE_ERROR:
		return "error";
	case FSM_STATE_BOOT:
		return "boot";
	case FSM_STATE_PREPARE:
		return "prepare";
	case FSM_STATE_RESET_LOOP:
		return "reset_loop";
	case FSM_STATE_RETRY_DELAY:
		return "retry_delay";
	case FSM_STATE_ATTACH:
		return "attach";
	case FSM_STATE_OPEN_SOCKET:
		return "open_socket";
	case FSM_STATE_READY:
		return "ready";
	case FSM_STATE_SLEEP:
		return "sleep";
	case FSM_STATE_WAKEUP:
		return "wakeup";
	case FSM_STATE_SEND:
		return "send";
	case FSM_STATE_RECEIVE:
		return "receive";
	case FSM_STATE_CONEVAL:
		return "coneval";
	case FSM_STATE_GNSS:
		return "gnss";
	}
	return "unknown";
}

const char *ctr_lte_v2_get_state(void)
{
	return fsm_state_str(m_state);
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
	if (event == CTR_LTE_V2_EVENT_CSCON_1) {
		m_start_cscon1 = k_uptime_get_32();
	} else if (event == CTR_LTE_V2_EVENT_CSCON_0) {
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.cscon_1_last_duration_ms = k_uptime_get_32() - m_start_cscon1;
		m_metrics.cscon_1_duration_ms += m_metrics.cscon_1_last_duration_ms;
		k_mutex_unlock(&m_metrics_lock);
	}

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
	LOG_INF("event: %s, state: %s", ctr_lte_v2_str_fsm_event(event), fsm_state_str(m_state));

	struct fsm_state_desc *fsm_state = get_fsm_state(m_state);
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

static inline int leaving_state(enum fsm_state next)
{
	struct fsm_state_desc *fsm_state = get_fsm_state(m_state);
	if (fsm_state && fsm_state->on_leave) {
		LOG_INF("%s", fsm_state_str(m_state));
		int ret = fsm_state->on_leave();
		if (ret < 0) {
			LOG_WRN("failed to leave state, error: %i", ret);
			if (next != FSM_STATE_ERROR) {
				delegate_event(CTR_LTE_V2_EVENT_ERROR);
			}
			return ret;
		}
	}
	return 0;
}

static void entering_state(struct fsm_state_desc *next_desc)
{
	LOG_INF("%s", fsm_state_str(next_desc->state));

	m_state = next_desc->state;
	if (next_desc->on_enter) {
		int ret = next_desc->on_enter();
		if (ret < 0) {
			LOG_WRN("failed to enter state error: %i", ret);
			if (next_desc->state != FSM_STATE_ERROR) {
				delegate_event(CTR_LTE_V2_EVENT_ERROR);
			}
			return;
		}
	}
}

static void transition_state(enum fsm_state next)
{
	if (next == m_state) {
		LOG_INF("no-op: already in %s", fsm_state_str(next));
		return;
	}

	struct fsm_state_desc *next_desc = get_fsm_state(next);
	if (!next_desc) {
		LOG_ERR("Unknown state: %s %d", fsm_state_str(next), next);
		return;
	}

	if (next == FSM_STATE_ERROR && m_state == FSM_STATE_ERROR) {
		LOG_WRN("Already in error state, ignoring");
		return;
	}

	if (m_state == FSM_STATE_ERROR) {
		m_error_ctx.prev_state = m_state; /* Save previous state before error */
	}

	if (leaving_state(next) == 0) {
		entering_state(next_desc);
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

int ctr_lte_v2_reconnect(void)
{
	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("LTE Test mode enabled");
		return -ENOTSUP;
	}

	if (m_state == FSM_STATE_DISABLED) {
		LOG_WRN("Cannot reconnect, LTE is disabled");
		return -ENODEV;
	}

	stop_timer();

	k_work_cancel(&m_event_dispatch_work);
	k_mutex_lock(&m_event_rb_lock, K_FOREVER);
	ring_buf_reset(&m_event_rb);
	k_mutex_unlock(&m_event_rb_lock);

	transition_state(FSM_STATE_DISABLED);

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
	LOG_INF("send_len: %u", param->send_len);

	k_timepoint_t end = sys_timepoint_calc(param->timeout);

	k_mutex_lock(&m_send_recv_lock, sys_timepoint_timeout(end));

	LOG_DBG("locked");

	m_send_recv_param = (struct ctr_lte_v2_send_recv_param *)param;
	m_send_recv_result = -ETIMEDOUT;

	k_event_clear(&m_states_event, SEND_RECV_DONE_BIT | SEND_RECV_STOP_BIT);

	/* Set flags to indicate pending work */
	atomic_set_bit(&m_flag, FLAG_SEND_PENDING);
	if (param->recv_buf) {
		atomic_set_bit(&m_flag, FLAG_RECV_PENDING);
	}

	delegate_event(CTR_LTE_V2_EVENT_SEND);

	LOG_DBG("waiting for end transaction");

	uint32_t events = k_event_wait(&m_states_event, SEND_RECV_DONE_BIT, false,
				       sys_timepoint_timeout(end));

	int result = m_send_recv_result;

	if (!events) {
		/* Timeout - wait for FSM to stop working on SEND/RECEIVE */
		LOG_WRN("send_recv timeout, waiting for FSM to complete");
		events = k_event_wait(&m_states_event, SEND_RECV_STOP_BIT, false, FSM_STOP_TIMEOUT);
		if (!events) {
			LOG_ERR("FSM stuck - SEND_RECV_STOP_BIT not received");
			/* Note: Save result BEFORE delegate_event because FSM error handler
			 * will overwrite m_send_recv_result with -EIO. We want -EFAULT. */
			result = -EFAULT;
			delegate_event(CTR_LTE_V2_EVENT_ERROR);
		} else {
			LOG_DBG("FSM completed after timeout");
			result = m_send_recv_result;
		}
	}

	/* Clear flags - only this function clears them */
	atomic_clear_bit(&m_flag, FLAG_SEND_PENDING);
	atomic_clear_bit(&m_flag, FLAG_RECV_PENDING);
	m_send_recv_param = NULL;

	k_mutex_unlock(&m_send_recv_lock);

	LOG_DBG("unlock, result: %d", result);

	return result;
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
	if (enable) {
		atomic_set_bit(&m_flag, FLAG_GNSS_ENABLE);
		delegate_event(CTR_LTE_V2_EVENT_XGPS_ENABLE);
	} else {
		atomic_clear_bit(&m_flag, FLAG_GNSS_ENABLE);
		delegate_event(CTR_LTE_V2_EVENT_XGPS_DISABLE);
	}
	return 0;
}

int ctr_lte_v2_gnss_get_enable(bool *enable)
{
	if (!enable) {
		return -EINVAL;
	}
	*enable = atomic_test_bit(&m_flag, FLAG_GNSS_ENABLE);
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
		transition_state(FSM_STATE_BOOT);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_disabled(void)
{
	memset(&m_error_ctx, 0, sizeof(m_error_ctx));
	m_attach_retry_count = 0;

	atomic_clear_bit(&m_flag, FLAG_CSCON);
	atomic_clear_bit(&m_flag, FLAG_CFUN4);

	int ret = ctr_lte_v2_flow_rfmux_acquire();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_rfmux_acquire` failed: %d", ret);
		return ret;
	}
	return 0;
}

static int on_enter_error(void)
{
	m_error_ctx.on_timeout_state = FSM_STATE_BOOT; /* Default timeout state */

	int ret = ctr_lte_v2_flow_check();
	if (ret == 0) {
		m_error_ctx.flow_check_failures = 0;
		LOG_INF("Flow check successful, resuming operation in 5 seconds");
		m_error_ctx.on_timeout_state = FSM_STATE_READY;
		start_timer(K_SECONDS(5));
		return 0;
	}

	k_event_clear(&m_states_event, CONNECTED_BIT);

	if (ret == -ENOTSOCK) {
		m_error_ctx.flow_check_failures++;

		LOG_INF("failed (attempt %d/%d): %d", m_error_ctx.flow_check_failures,
			ON_ERROR_MAX_FLOW_CHECK_RETRIES, ret);

		if (m_error_ctx.flow_check_failures < ON_ERROR_MAX_FLOW_CHECK_RETRIES) {
			LOG_ERR("Socket error, retrying open in 5 seconds");
			m_error_ctx.on_timeout_state = FSM_STATE_OPEN_SOCKET;
			start_timer(K_SECONDS(5));
			return 0;
		}

		LOG_ERR("Max retries reached, taking fallback action");
		m_error_ctx.flow_check_failures = 0; /* Reset counter */
	}

	ret = ctr_lte_v2_flow_disable(true);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
	}

	m_error_ctx.timeout_s += 10;
	if (m_error_ctx.timeout_s > 600) { /* Max 10 minutes */
		m_error_ctx.timeout_s = 600;
	}

	LOG_INF("Waiting %u seconds before next reconnect attempt", m_error_ctx.timeout_s);
	start_timer(K_SECONDS(m_error_ctx.timeout_s));

	return 0;
}

static int error_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		transition_state(m_error_ctx.on_timeout_state);
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
		transition_state(FSM_STATE_PREPARE);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		__fallthrough;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		transition_state(FSM_STATE_RESET_LOOP);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		__fallthrough;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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

	m_attach_retry_count = 0;

	return 0;
}

static int reset_loop_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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

	struct ctr_lte_v2_attach_timeout timeout = get_attach_timeout(m_attach_retry_count);

	LOG_INF("Waiting %d sec before attach retry",
		k_ticks_to_sec_floor32(timeout.retry_delay.ticks));

	start_timer(timeout.retry_delay);

	m_start = k_uptime_get_32();
	return 0;
}

static int retry_delay_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_TIMEOUT:
		transition_state(FSM_STATE_BOOT);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_retry_delay(void)
{
	stop_timer();
	m_attach_retry_count++; /* Increment retry count */
	return 0;
}

static int on_enter_attach(void)
{
	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	m_metrics.attach_count++;
	int ret = ctr_rtc_get_ts(&m_metrics.attach_last_ts);
	if (ret) {
		LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
	}
	k_mutex_unlock(&m_metrics_lock);

	m_start = k_uptime_get_32();

	k_event_clear(&m_states_event, CONNECTED_BIT);

	struct ctr_lte_v2_attach_timeout timeout = get_attach_timeout(m_attach_retry_count);

	LOG_INF("Try to attach with timeout %d sec",
		k_ticks_to_sec_floor32(timeout.attach_timeout.ticks));

	start_timer(timeout.attach_timeout);

	return 0;
}

static int attach_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_REGISTERED:
		m_attach_retry_count = 0;
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.attach_last_duration_ms = k_uptime_get_32() - m_start;
		m_metrics.attach_duration_ms += m_metrics.attach_last_duration_ms;
		k_mutex_unlock(&m_metrics_lock);
		transition_state(FSM_STATE_OPEN_SOCKET);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		atomic_set_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		transition_state(FSM_STATE_RESET_LOOP);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.attach_fail_count++;
		m_metrics.attach_last_duration_ms = k_uptime_get_32() - m_start;
		m_metrics.attach_duration_ms += m_metrics.attach_last_duration_ms;
		k_mutex_unlock(&m_metrics_lock);
		transition_state(FSM_STATE_RETRY_DELAY);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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
	k_event_clear(&m_states_event, CONNECTED_BIT);

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
		m_error_ctx.timeout_s = 0; /* Reset error timeout counter on success */
		transition_state(FSM_STATE_CONEVAL);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		atomic_set_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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
	else if (atomic_test_bit(&m_flag,
				 FLAG_GNSS_ENABLE)) { /* Only one event delegate at a time */
		delegate_event(CTR_LTE_V2_EVENT_XGPS_ENABLE);
	}
#endif
	else {
		int ret = ctr_lte_v2_state_get_cereg_param(&m_cereg_param);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_state_get_cereg_param` failed: %d", ret);
			return ret;
		}
		if (m_cereg_param.active_time == -1) { /* PSM is not supported */
			start_timer(K_MSEC(500));
		}
	}

	return 0;
}

static int ready_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_SEND:
		if (atomic_test_bit(&m_flag, FLAG_CFUN4)) {
			return 0; /* ignore SEND event */
		}
		stop_timer();
		int ret = ctr_lte_v2_flow_check();
		if (ret) {
			return ret;
		}
		transition_state(FSM_STATE_SEND);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		if (atomic_test_bit(&m_flag, FLAG_CFUN4)) {
			return 0; /* ignore DEREGISTERED event */
		}
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		atomic_set_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_XMODMSLEEEP:
#if defined(CONFIG_CTR_LTE_V2_GNSS)
		__fallthrough;
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		if (atomic_test_bit(&m_flag, FLAG_GNSS_ENABLE)) {
			transition_state(FSM_STATE_GNSS);
			break;
		}
#endif
		transition_state(FSM_STATE_SLEEP);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		if (m_cereg_param.active_time == -1) { /* if PSM is not supported */
			LOG_WRN("PSM is not supported, disabling LTE modem to save power");
			int ret = ctr_lte_v2_flow_close_socket();
			if (ret) {
				LOG_ERR("Call `ctr_lte_v2_flow_close_socket` failed: %d", ret);
				return ret;
			}
			ret = ctr_lte_v2_flow_cfun(4);
			if (ret) {
				LOG_ERR("Call `ctr_lte_v2_flow_cfun` failed: %d", ret);
				return ret;
			}
			atomic_set_bit(&m_flag, FLAG_CFUN4);
			return 0;
		}
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_ready(void)
{
	stop_timer();
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
		__fallthrough;
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		transition_state(FSM_STATE_WAKEUP);
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
		if (atomic_test_bit(&m_flag, FLAG_CFUN4)) {
			int ret = ctr_lte_v2_flow_cfun(1); /* Exit flight mode */
			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_v2_flow_cfun` failed: %d", ret);
				return ret;
			}
			atomic_clear_bit(&m_flag, FLAG_CFUN4);
			transition_state(FSM_STATE_ATTACH);
			return 0;
		}
		transition_state(FSM_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		__fallthrough;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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

	if (!atomic_test_bit(&m_flag, FLAG_SEND_PENDING)) {
		delegate_event(CTR_LTE_V2_EVENT_READY);
		return 0;
	}

	k_event_clear(&m_states_event, SEND_RECV_STOP_BIT);

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

		m_send_recv_result = -EIO;

		return ret;
	}

	start_timer(SEND_CSCON_1_TIMEOUT);

	if (atomic_test_bit(&m_flag, FLAG_CSCON)) {
		delegate_event(CTR_LTE_V2_EVENT_SEND);
	}

	return 0;
}

static int send_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		m_send_recv_result = -ECONNRESET;
		k_event_post(&m_states_event, SEND_RECV_STOP_BIT);
		transition_state(FSM_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		atomic_set_bit(&m_flag, FLAG_CSCON); /* Continue to send */
		__fallthrough;
	case CTR_LTE_V2_EVENT_SEND:
		stop_timer();
		if (atomic_test_bit(&m_flag, FLAG_RECV_PENDING)) {
			LOG_INF("proceed to RECEIVE state");
			transition_state(FSM_STATE_RECEIVE);
		} else {
			m_send_recv_result = 0;
			atomic_clear_bit(&m_flag, FLAG_SEND_PENDING);
			atomic_clear_bit(&m_flag, FLAG_RECV_PENDING);
			k_event_post(&m_states_event, SEND_RECV_DONE_BIT | SEND_RECV_STOP_BIT);
			transition_state(FSM_STATE_CONEVAL);
		}
		break;
	case CTR_LTE_V2_EVENT_READY:
		__fallthrough;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.uplink_errors++;
		k_mutex_unlock(&m_metrics_lock);
		m_send_recv_result = -ETIMEDOUT;
		k_event_post(&m_states_event, SEND_RECV_STOP_BIT);
		transition_state(FSM_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		m_send_recv_result = -ENETDOWN;
		k_event_post(&m_states_event, SEND_RECV_STOP_BIT);
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		m_send_recv_result = -EIO;
		k_event_post(&m_states_event, SEND_RECV_STOP_BIT);
		transition_state(FSM_STATE_ERROR);
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
	if (!atomic_test_bit(&m_flag, FLAG_RECV_PENDING)) {
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
		LOG_ERR("Call `ctr_lte_v2_flow_recv` failed: %d", ret);
		k_mutex_lock(&m_metrics_lock, K_FOREVER);
		m_metrics.downlink_errors++;
		k_mutex_unlock(&m_metrics_lock);
		m_send_recv_result = -EIO;
		return ret;
	}

	k_mutex_lock(&m_metrics_lock, K_FOREVER);
	m_metrics.downlink_bytes += *m_send_recv_param->recv_len;
	k_mutex_unlock(&m_metrics_lock);

	delegate_event(CTR_LTE_V2_EVENT_RECV);

	m_send_recv_result = 0;
	atomic_clear_bit(&m_flag, FLAG_SEND_PENDING);
	atomic_clear_bit(&m_flag, FLAG_RECV_PENDING);
	k_event_post(&m_states_event, SEND_RECV_DONE_BIT | SEND_RECV_STOP_BIT);

	return 0;
}

static int receive_event_handler(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_RECV:
		transition_state(FSM_STATE_CONEVAL);
		break;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		m_send_recv_result = -ETIMEDOUT;
		__fallthrough;
	case CTR_LTE_V2_EVENT_READY:
		transition_state(FSM_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		m_send_recv_result = -ENETDOWN;
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		m_send_recv_result = -EIO;
		transition_state(FSM_STATE_ERROR);
		break;
	default:
		break;
	}
	return 0;
}

static int on_leave_receive(void)
{
	k_event_post(&m_states_event, SEND_RECV_STOP_BIT);
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
		__fallthrough;
	case CTR_LTE_V2_EVENT_TIMEOUT:
		transition_state(FSM_STATE_READY);
		break;
	case CTR_LTE_V2_EVENT_CSCON_0:
		atomic_clear_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_CSCON_1:
		atomic_set_bit(&m_flag, FLAG_CSCON);
		break;
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		transition_state(FSM_STATE_ATTACH);
		break;
	case CTR_LTE_V2_EVENT_ERROR:
		transition_state(FSM_STATE_ERROR);
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

	ret = ctr_lte_v2_state_get_cereg_param(&m_cereg_param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_state_get_cereg_param` failed: %d", ret);
		return ret;
	}

	/* IF PSM is disabled, Deactivates LTE without shutting down GNSS services. */
	if (m_cereg_param.active_time == -1) {
		ret = ctr_lte_v2_flow_cfun(20);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_cfun 20` failed: %d", ret);
			return ret;
		}
	}

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
	case CTR_LTE_V2_EVENT_XGPS:
		k_work_submit_to_queue(&m_work_q, &m_gnss_work);
		break;
	case CTR_LTE_V2_EVENT_SEND:
		__fallthrough;
	case CTR_LTE_V2_EVENT_XGPS_DISABLE:
		if (m_cereg_param.active_time == -1) {
			transition_state(FSM_STATE_ATTACH);
		} else {
			transition_state(FSM_STATE_READY);
		}
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

	/* IF PSM is disabled, Activates LTE without changing GNSS. */
	if (m_cereg_param.active_time == -1) {
		ret = ctr_lte_v2_flow_cfun(21);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_cfun 20` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}
#endif

/* clang-format off */
static struct fsm_state_desc m_fsm_states[] = {
	{FSM_STATE_DISABLED, on_enter_disabled, on_leave_disabled, disabled_event_handler},
	{FSM_STATE_ERROR, on_enter_error, NULL, error_event_handler},
	{FSM_STATE_BOOT, on_enter_boot, on_leave_boot, boot_event_handler},
	{FSM_STATE_PREPARE, on_enter_prepare, on_leave_prepare, prepare_event_handler},
	{FSM_STATE_RESET_LOOP, on_enter_reset_loop, on_leave_reset_loop, reset_loop_event_handler},
	{FSM_STATE_RETRY_DELAY, on_enter_retry_delay, on_leave_retry_delay, retry_delay_event_handler},
	{FSM_STATE_ATTACH, on_enter_attach, on_leave_attach, attach_event_handler},
	{FSM_STATE_OPEN_SOCKET, on_enter_open_socket, NULL, open_socket_event_handler},
	{FSM_STATE_READY, on_enter_ready, on_leave_ready, ready_event_handler},
	{FSM_STATE_SLEEP, on_enter_sleep, NULL, sleep_event_handler},
	{FSM_STATE_WAKEUP, on_enter_wakeup, on_leave_wakeup, wakeup_event_handler},
	{FSM_STATE_SEND, on_enter_send, on_leave_send, send_event_handler},
	{FSM_STATE_RECEIVE, on_enter_receive, on_leave_receive, receive_event_handler},
	{FSM_STATE_CONEVAL, on_enter_coneval, NULL, coneval_event_handler},
	#if defined(CONFIG_CTR_LTE_V2_GNSS)
	{FSM_STATE_GNSS, on_enter_gnss, on_leave_gnss, gnss_event_handler},
	#endif
};
/* clang-format on */

struct fsm_state_desc *get_fsm_state(enum fsm_state state)
{
	for (size_t i = 0; i < ARRAY_SIZE(m_fsm_states); i++) {
		if (m_fsm_states[i].state == state) {
			return &m_fsm_states[i];
		}
	}

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

	m_state = FSM_STATE_DISABLED;

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
