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

int ctr_lte_v2_flow_reset(void);
int ctr_lte_v2_flow_start(void);
int ctr_lte_v2_flow_stop(void);
int ctr_lte_v2_flow_wait_on_modem_sleep(k_timeout_t delay);
int ctr_lte_v2_flow_prepare(int retries, k_timeout_t delay);
int ctr_lte_v2_flow_attach(k_timeout_t timeout);
int ctr_lte_v2_flow_send_recv(int retries, k_timeout_t delay,
			      const struct ctr_lte_v2_send_recv_param *param, bool rai);
int ctr_lte_v2_flow_coneval(struct ctr_lte_v2_conn_param *param);

int ctr_lte_v2_flow_cmd_test_uart(const struct shell *shell, size_t argc, char **argv);
int ctr_lte_v2_flow_cmd_test_reset(const struct shell *shell, size_t argc, char **argv);
int ctr_lte_v2_flow_cmd_test_wakeup(const struct shell *shell, size_t argc, char **argv);
int ctr_lte_v2_flow_cmd_test_cmd(const struct shell *shell, size_t argc, char **argv);
int ctr_lte_v2_flow_cmd_trace(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_FLOW_H_ */
