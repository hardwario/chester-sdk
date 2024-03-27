/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_lte_v2_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
			  int *seconds);
int ctr_lte_v2_parse_xsocket_set(const char *s, int *handle, int *type, int *protocol);
int ctr_lte_v2_parse_urc_cereg(const char *s, struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_parse_urc_xmodemsleep(const char *s, int *p1, int *p2);
int ctr_lte_v2_parse_urc_coneval(const char *s, long *result, long *rrc_state,
				 long *energy_estimate, long *rsrp, long *rsrq, long *snr, int *cid,
				 char *plmn, size_t plmn_size, long *phys_cell_id, long *earfcn,
				 long *band, long *tau_triggered, long *ce_level, long *tx_power,
				 long *tx_repetitions, long *rx_repetitions, long *dl_pathloss);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_ */
