/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_parse.h"
#include "ctr_lte_tok.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int ctr_lte_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
		       int *seconds)
{
	/* +CCLK: "21/11/08,16:01:13+04" */

	char *p = (char *)s;

	if ((p = ctr_lte_tok_pfx(p, "+CCLK: ")) == NULL) {
		return -EINVAL;
	}

	bool def;
	char buf[20 + 1];

	if ((p = ctr_lte_tok_str(p, &def, buf, sizeof(buf))) == NULL) {
		return -EINVAL;
	}

	if (!def) {
		return -EINVAL;
	}

	if (strlen(buf) != 20) {
		return -EINVAL;
	}

	if (!isdigit((int)buf[0]) || !isdigit((int)buf[1]) || buf[2] != '/' ||
	    !isdigit((int)buf[3]) || !isdigit((int)buf[4]) || buf[5] != '/' ||
	    !isdigit((int)buf[6]) || !isdigit((int)buf[7]) || buf[8] != ',' ||
	    !isdigit((int)buf[9]) || !isdigit((int)buf[10]) || buf[11] != ':' ||
	    !isdigit((int)buf[12]) || !isdigit((int)buf[13]) || buf[14] != ':' ||
	    !isdigit((int)buf[15]) || !isdigit((int)buf[16]) ||
	    (buf[17] != '+' && buf[17] != '-') || !isdigit((int)buf[18]) ||
	    !isdigit((int)buf[19])) {
		return -EINVAL;
	}

	buf[2] = buf[5] = buf[8] = buf[11] = buf[14] = buf[17] = '\0';

	if (year != NULL) {
		*year = atoi(&buf[0]);
	}

	if (month != NULL) {
		*month = atoi(&buf[3]);
	}

	if (day != NULL) {
		*day = atoi(&buf[6]);
	}

	if (hours != NULL) {
		*hours = atoi(&buf[9]);
	}

	if (minutes != NULL) {
		*minutes = atoi(&buf[12]);
	}

	if (seconds != NULL) {
		*seconds = atoi(&buf[15]);
	}

	return 0;
}

int ctr_lte_parse_cereg(const char *s, int *stat)
{
	/* +CEREG: 5,"AF66","009DE067",9,,,"00000000","00111000" */

	char *p = (char *)s;

	if ((p = ctr_lte_tok_pfx(p, "+CEREG: ")) == NULL) {
		return -EINVAL;
	}

	bool def;
	long num;

	if ((p = ctr_lte_tok_num(p, &def, &num)) == NULL) {
		return -EINVAL;
	}

	if (stat != NULL) {
		*stat = num;
	}

	return 0;
}

int ctr_lte_parse_coneval(const char *s, long *result, long *rrc_state, long *energy_estimate,
			  long *rsrp, long *rsrq, long *snr, char *cell_id, size_t cell_id_size,
			  char *plmn, size_t plmn_size, long *phys_cell_id, long *earfcn,
			  long *band, long *tau_triggered, long *ce_level, long *tx_power,
			  long *tx_repetitions, long *rx_repetitions, long *dl_pathloss)
{
	/* %CONEVAL: 0,1,5,8,2,14,"011B0780”,"26295",7,1575,3,1,1,23,16,32,130 */

	char *p = (char *)s;

	if ((p = ctr_lte_tok_pfx(p, "%CONEVAL: ")) == NULL) {
		return -EINVAL;
	}

	bool def;

	if ((p = ctr_lte_tok_num(p, &def, result)) == NULL || !def) {
		return -EINVAL;
	}

	if (ctr_lte_tok_end(p) != NULL) {
		return 0;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, rrc_state)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, energy_estimate)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, rsrp)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, rsrq)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, snr)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_str(p, &def, cell_id, cell_id_size)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_str(p, &def, plmn, plmn_size)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, phys_cell_id)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, earfcn)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, band)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, tau_triggered)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, ce_level)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, tx_power)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, tx_repetitions)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, rx_repetitions)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_tok_num(p, &def, dl_pathloss)) == NULL || !def) {
		return -EINVAL;
	}

	if (ctr_lte_tok_end(p) == NULL) {
		return -EINVAL;
	}

	return 0;
}
