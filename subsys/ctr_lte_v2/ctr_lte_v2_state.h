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

/* The last extended error report (AT+CEER), such as "EMM 22" with the quotes: the module and the
 * cause value of the last failure the modem stored */
#define CTR_LTE_V2_STATE_CEER_SIZE 32

/* Copies the report into buf, cut to its size and NUL-terminated; -ENODATA when there is none */
int ctr_lte_v2_state_get_ceer(char *buf, size_t size);
void ctr_lte_v2_state_set_ceer(const char *ceer);

int ctr_lte_v2_state_get_conn_param(struct ctr_lte_v2_conn_param *param);
void ctr_lte_v2_state_set_conn_param(const struct ctr_lte_v2_conn_param *param);

/* A registration reject: an EMM cause (cause_type 0; 1 is manufacturer specific) while not
 * registered. A registration can be accepted with a cause too, e.g. 18 "CS domain not available"
 * for an EPS-only attach. */
static inline bool ctr_lte_v2_state_cereg_is_reject(const struct ctr_lte_v2_cereg_param *param)
{
	return param->cause_type == 0 && param->reject_cause &&
	       param->stat != CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME &&
	       param->stat != CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING;
}

int ctr_lte_v2_state_get_cereg_param(struct ctr_lte_v2_cereg_param *param);
void ctr_lte_v2_state_set_cereg_param(const struct ctr_lte_v2_cereg_param *param);
void ctr_lte_v2_state_add_cereg_event(const struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_state_get_cereg_history(struct ctr_lte_v2_cereg_event *events, size_t max,
				       size_t *count);
int ctr_lte_v2_state_get_last_reject(struct ctr_lte_v2_cereg_event *event);

int ctr_lte_v2_state_set_gnss_update(const struct ctr_lte_v2_gnss_update *update);
int ctr_lte_v2_state_get_gnss_update(struct ctr_lte_v2_gnss_update *update);

const char *ctr_lte_v2_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_STATE_H_ */
