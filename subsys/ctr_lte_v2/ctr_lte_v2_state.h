/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_STATE_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_STATE_H_

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_lte_v2_state_get_imei(uint64_t *imei);
void ctr_lte_v2_state_set_imei(uint64_t imei);

int ctr_lte_v2_state_get_imsi(uint64_t *imsi);
void ctr_lte_v2_state_set_imsi(uint64_t imsi);

int ctr_lte_v2_state_get_iccid(char **iccid);
void ctr_lte_v2_state_set_iccid(const char *iccid);

int ctr_lte_v2_state_get_modem_fw_version(char **version);
void ctr_lte_v2_state_set_modem_fw_version(const char *version);

int ctr_lte_v2_state_get_conn_param(struct ctr_lte_v2_conn_param *param);
void ctr_lte_v2_state_set_conn_param(const struct ctr_lte_v2_conn_param *param);

int ctr_lte_v2_state_get_cereg_param(struct ctr_lte_v2_cereg_param *param);
void ctr_lte_v2_state_set_cereg_param(const struct ctr_lte_v2_cereg_param *param);

int ctr_lte_v2_state_set_gnss_update(const struct ctr_lte_v2_gnss_update *update);
int ctr_lte_v2_state_get_gnss_update(struct ctr_lte_v2_gnss_update *update);

const char *ctr_lte_v2_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_STATE_H_ */
