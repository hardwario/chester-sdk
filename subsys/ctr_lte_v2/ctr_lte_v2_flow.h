/*
 * Copyright (c) 2023 HARDWARIO a.s.
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
	CTR_LTE_V2_EVENT_RECV,
	CTR_LTE_V2_EVENT_XGPS_ENABLE,
	CTR_LTE_V2_EVENT_XGPS_DISABLE,
	CTR_LTE_V2_EVENT_XGPS,
};

typedef void (*ctr_lte_v2_flow_event_delegate_cb)(enum ctr_lte_v2_event event);
typedef void (*ctr_lte_v2_flow_bypass_cb)(void *user_data, const uint8_t *data, size_t len);

int ctr_lte_v2_flow_init(ctr_lte_v2_flow_event_delegate_cb cb);
int ctr_lte_v2_flow_rfmux_acquire(void);
int ctr_lte_v2_flow_rfmux_release(void);
int ctr_lte_v2_flow_enable(bool wakeup);
int ctr_lte_v2_flow_disable(bool send_sleep);
int ctr_lte_v2_flow_reset(void);

int ctr_lte_v2_flow_prepare(void);
int ctr_lte_v2_flow_cfun(int cfun);
int ctr_lte_v2_flow_sim_info(void);
int ctr_lte_v2_flow_sim_fplmn(void);
int ctr_lte_v2_flow_open_socket(void);

int ctr_lte_v2_flow_check(void);
int ctr_lte_v2_flow_send(const struct ctr_lte_v2_send_recv_param *param);
int ctr_lte_v2_flow_recv(const struct ctr_lte_v2_send_recv_param *param);

int ctr_lte_v2_flow_coneval(void);

int ctr_lte_v2_flow_wake_up_and_wait_on_ready(void);
int ctr_lte_v2_flow_cmd_without_response(const char *s);
int ctr_lte_v2_flow_bypass_set_cb(ctr_lte_v2_flow_bypass_cb cb, void *user_data);
int ctr_lte_v2_flow_bypass_write(const uint8_t *data, const size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_FLOW_H_ */
