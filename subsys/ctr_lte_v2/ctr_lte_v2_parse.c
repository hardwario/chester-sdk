/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_parse.h"
#include "ctr_lte_v2_tok.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2_parse, CONFIG_CTR_LTE_V2_LOG_LEVEL);

int ctr_lte_v2_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
			  int *seconds)
{
	/* "21/11/08,16:01:13+04" */

	char *p = (char *)s;

	bool def;
	char buf[20 + 1];

	if ((p = ctr_lte_v2_tok_str(p, &def, buf, sizeof(buf))) == NULL) {
		LOG_ERR("ctr_lte_v2_tok_str failed");
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
		*year = strtoll(&buf[0], NULL, 10);
	}

	if (month != NULL) {
		*month = strtoll(&buf[3], NULL, 10);
	}

	if (day != NULL) {
		*day = strtoll(&buf[6], NULL, 10);
	}

	if (hours != NULL) {
		*hours = strtoll(&buf[9], NULL, 10);
	}

	if (minutes != NULL) {
		*minutes = strtoll(&buf[12], NULL, 10);
	}

	if (seconds != NULL) {
		*seconds = strtoll(&buf[15], NULL, 10);
	}

	return 0;
}

int ctr_lte_v2_parse_xsocket_set(const char *s, struct xsocket_set_param *param)
{
	/* 0,2,17 */
	// <handle>,<type>,<protocol>

	if (param == NULL) {
		return -EINVAL;
	}

	char *p = (char *)s;

	bool def;
	long num;

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->handle = num;

	if (param->handle < 0) {
		LOG_ERR("socket handle is negative");
		return -ENOTCONN;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->type = num;

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->protocol = num;

	return 0;
}

int ctr_lte_v2_parse_xsocket_get(const char *s, struct xsocket_get_param *param)
{
	/* 0,1,0,2,0 */
	//  <handle>,<family>,<role>,<type>,<cid>

	if (param == NULL) {
		return -EINVAL;
	}

	char *p = (char *)s;

	bool def;
	long num;

	// handle
	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->handle = num;

	if (param->handle < 0) {
		LOG_ERR("socket handle is negative");
		return -ENOTCONN;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	// family
	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->family = num;

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	// role
	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->role = num;

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	// type
	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->type = num;

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	// cid
	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->cid = num;

	return 0;
}

int cellid_hex2int(char *ci, size_t size, int *cid)
{
	if (ci == NULL || size != 9 || cid == NULL) {
		return -EINVAL;
	}

	uint8_t cell_id_buf[4];

	size_t n = hex2bin(ci, 8, cell_id_buf, sizeof(cell_id_buf));

	if (n != sizeof(cell_id_buf)) {
		return -EINVAL;
	}

	*cid = sys_get_be32(cell_id_buf);

	return 0;
}

int ctr_lte_v2_parse_urc_cereg(const char *s, struct ctr_lte_v2_cereg_param *param)
{
	/* 5,"AF66","009DE067",9,,,"00000000","00111000" */
	/* 2,"B4DC","000AE520",9 */

	memset(param, 0, sizeof(*param));

	char *p = (char *)s;

	bool def;
	long num;

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
		return -EINVAL;
	}

	param->stat = num;

	if (param->stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME ||
	    param->stat == CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING ||
	    param->stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING) {
		if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
			return -EINVAL;
		}

		if ((p = ctr_lte_v2_tok_str(p, &def, param->tac, sizeof(param->tac))) == NULL ||
		    !def) {
			return -EINVAL;
		}

		if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
			return -EINVAL;
		}

		char cell_id[8 + 1];

		if ((p = ctr_lte_v2_tok_str(p, &def, cell_id, sizeof(cell_id))) == NULL || !def) {
			return -EINVAL;
		}

		if (cellid_hex2int(cell_id, sizeof(cell_id), &param->cid) != 0) {
			return -EINVAL;
		}

		if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
			return -EINVAL;
		}

		if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL || !def) {
			return -EINVAL;
		}

		param->act = num;
	}

	param->valid = true;

	return 0;
}

int ctr_lte_v2_parse_urc_xmodemsleep(const char *s, int *p1, int *p2)
{
	/* 1,86399999 */
	/* 4 */

	char *p = (char *)s;

	bool def;
	long num;

	*p1 = 0;
	*p2 = 0;

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL) {
		return -EINVAL;
	}

	if (p1 != NULL) {
		*p1 = num;
	}

	if (ctr_lte_v2_tok_end(p) != NULL) {
		return 0;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, &num)) == NULL) {
		return -EINVAL;
	}

	if (p2 != NULL) {
		*p2 = num;
	}

	return 0;
}

static int parse_coneval(const char *s, long *result, long *rrc_state, long *energy_estimate,
			 long *rsrp, long *rsrq, long *snr, int *cid, char *plmn, size_t plmn_size,
			 long *phys_cell_id, long *earfcn, long *band, long *tau_triggered,
			 long *ce_level, long *tx_power, long *tx_repetitions, long *rx_repetitions,
			 long *dl_pathloss)
{
	/* 0,1,5,8,2,14,"011B0780”,"26295",7,1575,3,1,1,23,16,32,130 */
	char *p = (char *)s;

	bool def;

	if ((p = ctr_lte_v2_tok_num(p, &def, result)) == NULL || !def) {
		return -EINVAL;
	}

	if (ctr_lte_v2_tok_end(p) != NULL) {
		return 0;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, rrc_state)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, energy_estimate)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, rsrp)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, rsrq)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, snr)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	char cell_id[8 + 1];

	if ((p = ctr_lte_v2_tok_str(p, &def, cell_id, sizeof(cell_id))) == NULL || !def) {
		return -EINVAL;
	}

	if (cellid_hex2int(cell_id, sizeof(cell_id), cid) != 0) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_str(p, &def, plmn, plmn_size)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, phys_cell_id)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, earfcn)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, band)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, tau_triggered)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, ce_level)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, tx_power)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, tx_repetitions)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, rx_repetitions)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_num(p, &def, dl_pathloss)) == NULL || !def) {
		return -EINVAL;
	}

	if (ctr_lte_v2_tok_end(p) == NULL) {
		return -EINVAL;
	}

	return 0;
}

int ctr_lte_v2_parse_coneval(const char *s, struct ctr_lte_v2_conn_param *param)
{
	int ret;
	memset(param, 0, sizeof(*param));

	long result;
	long energy_estimate;
	long rsrp;
	long rsrq;
	long snr;
	int cid;
	char plmn[6 + 1];
	long earfcn;
	long band;
	long ce_level;

	ret = parse_coneval(s, &result, NULL, &energy_estimate, &rsrp, &rsrq, &snr, &cid, plmn,
			    sizeof(plmn), NULL, &earfcn, &band, NULL, &ce_level, NULL, NULL, NULL,
			    NULL);
	if (ret) {
		return ret;
	}

	param->result = result;

	if (param->result == 0) {
		param->eest = energy_estimate;
		param->ecl = ce_level;
		param->rsrp = rsrp - 140;
		param->rsrq = (rsrq - 39) / 2;
		param->snr = snr - 24;
		param->plmn = strtol(plmn, NULL, 10);
		param->cid = cid;
		param->band = band;
		param->earfcn = earfcn;
		param->valid = true;
	}

	return 0;
}

int ctr_lte_v2_parse_urc_xgps(const char *s, struct ctr_lte_v2_gnss_update *update)
{

	// <latitude>,<longitude>,<altitude>,<accuracy>,<speed>,<heading>,<datetime>
	//           49.256682,17.699627,292.599670,5.468742,0.165512,73.682823,"2024-06-27
	//           16:06:52"

	memset(update, 0, sizeof(*update));

	char *p = (char *)s;

	bool def;

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->latitude)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->longitude)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->altitude)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->accuracy)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->speed)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_float(p, &def, &update->heading)) == NULL || !def) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_sep(p)) == NULL) {
		return -EINVAL;
	}

	if ((p = ctr_lte_v2_tok_str(p, &def, update->datetime, sizeof(update->datetime))) == NULL ||
	    !def) {
		return -EINVAL;
	}

	if (ctr_lte_v2_tok_end(p) == NULL) {
		return -EINVAL;
	}

	return 0;
}

// #XGPS: 49.256682,17.699627,292.599670,5.468742,0.165512,73.682823,"2024-06-27 16:06:52"
// #XGPS: 1,1
// #XGPS: 1,0
