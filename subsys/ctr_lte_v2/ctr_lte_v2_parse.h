/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* NCS includes (the conversion macros only, the modem library is not used) */
#include <modem/modem_info.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xsocket_set_param {
	int handle;
	int type;
	int protocol;
};

struct xsocket_get_param {
	int handle;
	int family;
	int role;
	int type;
	int cid;
};

struct cgdcont_param {
	int cid;          /* CID (context ID) */
	char pdn_type[9]; /* "IP" or "IPV6" */
	char apn[64];     /* Access Point Name */
	char addr[16];    /* IPv4 address */
};

#define XSOCKET_PROTOCOL_UDP 17
#define XSOCKET_TYPE_DGRAM   2
#define XSOCKET_ROLE_CLIENT  0
#define XSOCKET_FAMILY_IPV4  1

#define GRPS_TIMER_DEACTIVATED -1
#define GRPS_TIMER_INVALID     -2

int ctr_lte_v2_parse_xsocket_set(const char *s, struct xsocket_set_param *param);
int ctr_lte_v2_parse_xsocket_get(const char *s, struct xsocket_get_param *param);
/* True for what follows "+CEREG: " in a URC (the status, then nothing or the quoted TAC), false
 * for the response to AT+CEREG? (<n>,<stat>,...), which the URC handler gets as well */
bool ctr_lte_v2_parse_cereg_is_urc(const char *s);
int ctr_lte_v2_parse_urc_cereg(const char *s, struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_parse_urc_xmodemsleep(const char *s, int *p1, int *p2);
int ctr_lte_v2_parse_coneval(const char *s, struct ctr_lte_v2_conn_param *param);
int ctr_lte_v2_parse_urc_xgps(const char *s, struct ctr_lte_v2_gnss_update *update);
int ctr_lte_v2_parse_cgcont(const char *s, struct cgdcont_param *param);

/* The RSRP index the modem reports (%CONEVAL, %NCELLMEAS) in dBm: RSRP_IDX_TO_DBM of nrf, the
 * lower bound of the 1 dB step (index - 141, a negative index - 140) */
static inline int ctr_lte_v2_parse_rsrp_dbm(int rsrp)
{
	return RSRP_IDX_TO_DBM(rsrp);
}

/* The RSRQ index the modem reports (%CONEVAL, %NCELLMEAS) in dB: RSRQ_IDX_TO_DB of nrf, a float
 * in half dB, truncated toward zero (as ecid_signal_meas_info.c of nrf stores it in an s32) */
static inline int ctr_lte_v2_parse_rsrq_db(int rsrq)
{
	return (int)RSRQ_IDX_TO_DB(rsrq);
}

#if defined(CONFIG_CTR_LTE_V2_SCAN)
/* Validates a PLMN: 5 or 6 digits, MCC not 000 (-EBADMSG, -EPROTO) */
int ctr_lte_v2_parse_plmn(const char *s);
/* Parses what follows "%NCELLMEAS: " (GCI search); a full capacity is not an error, the cells and
 * neighbours that fit are kept. On an error the cells are cleared, result and uptime_s are left
 * to the caller. */
int ctr_lte_v2_parse_urc_ncellmeas(const char *s, struct ctr_lte_v2_ncellmeas_param *param);
/* Parses what follows "+COPS: " in the response to AT+COPS=?. Stores at most max networks, counts
 * them all; the names are skipped and the lists of the supported modes and formats after ",,"
 * are ignored. The caller owns the array, so the line needs no copy. */
int ctr_lte_v2_parse_cops_list(const char *s, struct ctr_lte_v2_scan_network *networks, size_t max,
			       size_t *count);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_ */
