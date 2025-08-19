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
#include <stdio.h>

LOG_MODULE_REGISTER(ctr_lte_v2_parse, CONFIG_CTR_LTE_V2_LOG_LEVEL);

int ctr_lte_v2_parse_xsocket_set(const char *s, struct xsocket_set_param *param)
{
	/* 0,2,17 */
	/* <handle>,<type>,<protocol> */

	if (!s || !param) {
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));

	const char *p = s;

	bool def;
	long num;

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->handle = num;

	if (param->handle < 0) {
		LOG_ERR("socket handle is negative");
		return -ENOTCONN;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->type = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->protocol = num;

	return 0;
}

int ctr_lte_v2_parse_xsocket_get(const char *s, struct xsocket_get_param *param)
{
	/* 0,1,0,2,0 */
	/* <handle>,<family>,<role>,<type>,<cid> */

	if (!s || !param) {
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));

	const char *p = s;

	bool def;
	long num;

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->handle = num;

	if (param->handle < 0) {
		LOG_ERR("socket handle is negative");
		return -ENOTCONN;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->family = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->role = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->type = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->cid = num;

	return 0;
}

int cellid_hex2int(char *ci, size_t size, int *cid)
{
	if (!ci || size != 9 || !cid) {
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

int parse_gprs_timer(const char *binary_string, int flag)
{
	if (strlen(binary_string) != 8) {
		return GRPS_TIMER_INVALID;
	}

	int byte_value = (int)strtol(binary_string, NULL, 2);
	int time_unit = (byte_value >> 5) & 0x07;
	int timer_value = byte_value & 0x1F;

	if (time_unit == 0b111) {
		return GRPS_TIMER_DEACTIVATED;
	}

	int multiplier = 0;
	if (flag == 2) {
		/* GPRS Timer 2 (Active-Time) */
		switch (time_unit) {
		case 0b000:
			multiplier = 2;
			break; /* 2 seconds */
		case 0b001:
			multiplier = 60;
			break; /* 1 minute */
		case 0b010:
			multiplier = 360;
			break; /* 6 minutes */
		default:
			return GRPS_TIMER_INVALID;
		}
	} else if (flag == 3) {
		/* GPRS Timer 3 (Periodic-TAU-ext) */
		switch (time_unit) {
		case 0b000:
			multiplier = 600;
			break; /* 10 minutes */
		case 0b001:
			multiplier = 3600;
			break; /* 1 hour */
		case 0b010:
			multiplier = 36000;
			break; /* 10 hours */
		case 0b011:
			multiplier = 2;
			break; /* 2 seconds */
		case 0b100:
			multiplier = 30;
			break; /* 30 seconds */
		case 0b101:
			multiplier = 60;
			break; /* 1 minute */
		case 0b110:
			multiplier = 1152000;
			break; /* 320 hours */
		default:
			return GRPS_TIMER_INVALID;
		}
	} else {
		return GRPS_TIMER_INVALID;
	}

	return timer_value * multiplier;
}

int ctr_lte_v2_parse_urc_cereg(const char *s, struct ctr_lte_v2_cereg_param *param)
{ /*
	 <stat>[,[<tac>],[<ci>],[<AcT>][,<cause_type>],[<reject_cause>][,[<Active-Time>],[<Periodic-TAU-ext>]]]]
	 5,"AF66","009DE067",9,,,"00000000","00111000"
	 2,"B4DC","000AE520",9
	 */

	if (!s || !param) {
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));

	const char *p = s;

	bool def;
	long num;
	char str[8 + 1];

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->stat = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		param->valid = true;
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, param->tac, sizeof(param->tac))) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, str, sizeof(str))) || !def) {
		return -EINVAL;
	}

	if (cellid_hex2int(str, sizeof(str), &param->cid) != 0) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		return -EINVAL;
	}

	param->act = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		param->valid = true;
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) && !def) {
		return -EINVAL;
	}

	param->cause_type = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) && !def) {
		return -EINVAL;
	}

	param->reject_cause = num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		param->valid = true;
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, str, sizeof(str))) || !def) {
		return -EINVAL;
	}

	param->active_time = parse_gprs_timer(str, 2);

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, str, sizeof(str))) || !def) {
		return -EINVAL;
	}

	param->periodic_tau_ext = parse_gprs_timer(str, 3);

	if (!ctr_lte_v2_tok_end(p)) {
		return -EINVAL;
	}

	param->valid = true;

	return 0;
}

int ctr_lte_v2_parse_urc_xmodemsleep(const char *s, int *p1, int *p2)
{
	/* 1,86399999 */
	/* 4 */

	if (!s) {
		return -EINVAL;
	}

	const char *p = s;

	bool def;
	long num;

	*p1 = 0;
	*p2 = 0;

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num))) {
		return -EINVAL;
	}

	if (p1) {
		*p1 = num;
	}

	if (ctr_lte_v2_tok_end(p)) {
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num))) {
		return -EINVAL;
	}

	if (p2) {
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
	const char *p = s;

	bool def;

	if (!(p = ctr_lte_v2_tok_num(p, &def, result)) || !def) {
		return -EINVAL;
	}

	if (ctr_lte_v2_tok_end(p)) {
		return 0;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, rrc_state)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, energy_estimate)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, rsrp)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, rsrq)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, snr)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	char cell_id[8 + 1];

	if (!(p = ctr_lte_v2_tok_str(p, &def, cell_id, sizeof(cell_id))) || !def) {
		return -EINVAL;
	}

	if (cellid_hex2int(cell_id, sizeof(cell_id), cid) != 0) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, plmn, plmn_size)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, phys_cell_id)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, earfcn)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, band)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, tau_triggered)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, ce_level)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, tx_power)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, tx_repetitions)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, rx_repetitions)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_num(p, &def, dl_pathloss)) || !def) {
		return -EINVAL;
	}

	if (!ctr_lte_v2_tok_end(p)) {
		return -EINVAL;
	}

	return 0;
}

int ctr_lte_v2_parse_coneval(const char *s, struct ctr_lte_v2_conn_param *param)
{
	int ret;

	if (!s || !param) {
		return -EINVAL;
	}

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

	/* <latitude>,<longitude>,<altitude>,<accuracy>,<speed>,<heading>,<datetime> */
	/* 49.256682,17.699627,292.599670,5.468742,0.165512,73.682823,"2024-06-27 16:06:52" */

	if (!s || !update) {
		return -EINVAL;
	}

	memset(update, 0, sizeof(*update));

	const char *p = s;

	bool def;

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->latitude)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->longitude)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->altitude)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->accuracy)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->speed)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_float(p, &def, &update->heading)) || !def) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, update->datetime, sizeof(update->datetime))) ||
	    !def) {
		return -EINVAL;
	}

	if (!ctr_lte_v2_tok_end(p)) {
		return -EINVAL;
	}

	return 0;
}

int ctr_lte_v2_parse_cgcont(const char *s, struct cgdcont_param *param)
{
	/* 0,"IP","iot.1nce.net","10.52.2.149",0,0 */
	if (!s || !param) {
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));
	param->cid = -1; /* Default CID is -1 (not set) */

	const char *p = s;

	bool def;
	long num;

	if (!(p = ctr_lte_v2_tok_num(p, &def, &num)) || !def) {
		LOG_ERR("Failed to parse CID");
		return -EINVAL;
	}

	param->cid = (int)num;

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		LOG_ERR("Failed to parse PDN type");
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, param->pdn_type, sizeof(param->pdn_type))) || !def) {
		LOG_ERR("Failed to parse PDN type");
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		LOG_ERR("Failed to parse APN");
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, param->apn, sizeof(param->apn))) || !def) {
		LOG_ERR("Failed to parse APN");
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_sep(p))) {
		LOG_ERR("Failed to parse address");
		return -EINVAL;
	}

	if (!(p = ctr_lte_v2_tok_str(p, &def, param->addr, sizeof(param->addr))) || !def) {
		LOG_ERR("Failed to parse address");
		return -EINVAL;
	}

	return 0;
}
