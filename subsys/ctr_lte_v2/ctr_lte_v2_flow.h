/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_FLOW_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_FLOW_H_

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_v2_event {
	CTR_LTE_V2_EVENT_ERROR = 0,
	CTR_LTE_V2_EVENT_TIMEOUT,
	CTR_LTE_V2_EVENT_ENABLE,
	CTR_LTE_V2_EVENT_RECONNECT,
	CTR_LTE_V2_EVENT_READY,
	CTR_LTE_V2_EVENT_SIMDETECTED,
	CTR_LTE_V2_EVENT_REGISTERED,
	CTR_LTE_V2_EVENT_DEREGISTERED,
	CTR_LTE_V2_EVENT_RESET_LOOP,
	CTR_LTE_V2_EVENT_SOCKET_OPENED,
	CTR_LTE_V2_EVENT_XMODMSLEEEP,
	CTR_LTE_V2_EVENT_CSCON_0,
	CTR_LTE_V2_EVENT_CSCON_1,
	CTR_LTE_V2_EVENT_XTIME,
	CTR_LTE_V2_EVENT_SEND,
	CTR_LTE_V2_EVENT_SEND_ABORT,
	CTR_LTE_V2_EVENT_RECV,
	CTR_LTE_V2_EVENT_XGPS_ENABLE,
	CTR_LTE_V2_EVENT_XGPS_DISABLE,
	CTR_LTE_V2_EVENT_XGPS,
	CTR_LTE_V2_EVENT_NCELLMEAS,
	CTR_LTE_V2_EVENT_NCELLMEAS_DONE,
	CTR_LTE_V2_EVENT_SCAN_TIMER,
	CTR_LTE_V2_EVENT_SCAN,
};

/**
 * @brief Attach/retry timeout configuration.
 */
struct ctr_lte_v2_attach_timeout {
	k_timeout_t attach_timeout; /**< Max duration of the current attach attempt. */
	k_timeout_t retry_delay;    /**< Delay before the *next* attempt (after a failure). */
};

typedef void (*ctr_lte_v2_flow_event_delegate_cb)(enum ctr_lte_v2_event event);
typedef void (*ctr_lte_v2_flow_bypass_cb)(void *user_data, const uint8_t *data, size_t len);

int ctr_lte_v2_flow_init(ctr_lte_v2_flow_event_delegate_cb cb);
int ctr_lte_v2_flow_rfmux_acquire(void);
int ctr_lte_v2_flow_rfmux_release(void);
int ctr_lte_v2_flow_enable(bool wakeup);
int ctr_lte_v2_flow_disable(bool send_sleep);
int ctr_lte_v2_flow_reset(void);

/* The mode as 'lte config mode' ("lte-m", "nb-iot" or both) sets %XSYSTEMMODE */
int ctr_lte_v2_flow_prepare(const char *mode);
/* %XSYSTEMMODE alone, the mode as for the prepare */
int ctr_lte_v2_flow_system_mode(const char *mode);
int ctr_lte_v2_flow_cfun(int cfun);
/* As ctr_lte_v2_flow_cfun, the response awaited up to the timeout instead of 30 s */
int ctr_lte_v2_flow_cfun_timeout(int cfun, k_timeout_t timeout);
int ctr_lte_v2_flow_sim_info(void);
int ctr_lte_v2_flow_sim_fplmn(void);
int ctr_lte_v2_flow_open_socket(void);
int ctr_lte_v2_flow_close_socket(void);

int ctr_lte_v2_flow_check(void);
/* xsend_issued is set once the AT#XSEND line goes out: the modem may then send the data even if
 * the dialog fails */
int ctr_lte_v2_flow_send(const struct ctr_lte_v2_send_recv_param *param, bool *xsend_issued);
/* -ENODATA: no data within the receive window (a lost reply, the socket works) */
int ctr_lte_v2_flow_recv(const struct ctr_lte_v2_send_recv_param *param);
int ctr_lte_v2_flow_rai_no_data(void);

int ctr_lte_v2_flow_coneval(void);
/* Reads the extended error report (AT+CEER) into the state, when the modem has one */
int ctr_lte_v2_flow_ceer(void);

int ctr_lte_v2_flow_wake_up_and_wait_on_ready(void);
int ctr_lte_v2_flow_cmd_without_response(const char *s);
#if defined(CONFIG_CTR_LTE_V2_SCAN)
/* AT%NCELLMEAS=5,7 (GCI search); the URC comes later. The line of a refusal into err */
int ctr_lte_v2_flow_ncellmeas(char *err, size_t err_size);
/* Test mode, without the FSM: armed, AT%NCELLMEAS=5,7 (refused: -EIO stored) */
int ctr_lte_v2_flow_test_ncellmeas_start(char *err, size_t err_size);
/* 0 once its URC came, -EAGAIN */
int ctr_lte_v2_flow_test_ncellmeas_wait(k_timeout_t timeout);
/* Without the URC (urc false) stopped, with a window for the cells found so far. The result
 * stored as the FSM's: 0, -EBADMSG or -ETIMEDOUT */
int ctr_lte_v2_flow_test_ncellmeas_end(bool urc);
/* Test mode: the UART on and the modem woken (Ready, bounded) unless the UART is on already; on
 * an error the UART is off again */
int ctr_lte_v2_flow_test_link_up(void);
/* Test mode: AT+CEREG=5 and AT+CSCON=1, for the URCs the scan waits for (after each CFUN=0) */
int ctr_lte_v2_flow_test_subscribe(void);
/* Test mode: forget the RRC connection and the registration of the URCs so far (before CFUN=1) */
void ctr_lte_v2_flow_test_attach_begin(void);
/* Test mode, since the begin: true once the RRC connection is open (connected) or released, false
 * at the timeout */
bool ctr_lte_v2_flow_test_wait_cscon(bool connected, k_timeout_t timeout);
/* Test mode, since the begin: true once registered (+CEREG stat 1 or 5), false at the timeout */
bool ctr_lte_v2_flow_test_wait_registered(k_timeout_t timeout);
/* AT%NCELLMEASSTOP; during a measurement the modem sends the cells found so far right after OK */
int ctr_lte_v2_flow_ncellmeas_stop(void);
/* AT+COPS=? into the scan result: 0, -ETIMEDOUT, -EIO (rejected, no network), -EBADMSG */
int ctr_lte_v2_flow_scan_networks(void);
#endif
int ctr_lte_v2_flow_bypass_set_cb(ctr_lte_v2_flow_bypass_cb cb, void *user_data);
int ctr_lte_v2_flow_bypass_write(const uint8_t *data, const size_t len);

struct ctr_lte_v2_attach_timeout ctr_lte_v2_flow_attach_policy_periodic(int attempt,
									k_timeout_t pause);
struct ctr_lte_v2_attach_timeout ctr_lte_v2_flow_attach_policy_progressive(int attempt);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_FLOW_H_ */
