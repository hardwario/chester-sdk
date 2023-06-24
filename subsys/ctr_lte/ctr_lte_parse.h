/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_PARSE_H_
#define CHESTER_SUBSYS_CTR_LTE_PARSE_H_

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_lte_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
		       int *seconds);
int ctr_lte_parse_cereg(const char *s, int *stat);
int ctr_lte_parse_coneval(const char *s, long *result, long *rrc_state, long *energy_estimate,
			  long *rsrp, long *rsrq, long *snr, char *cell_id, size_t cell_id_size,
			  char *plmn, size_t plmn_size, long *phys_cell_id, long *earfcn,
			  long *band, long *tau_triggered, long *ce_level, long *tx_power,
			  long *tx_repetitions, long *rx_repetitions, long *dl_pathloss);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_PARSE_H_ */
