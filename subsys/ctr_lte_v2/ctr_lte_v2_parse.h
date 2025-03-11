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

#define XSOCKET_PROTOCOL_UDP 17
#define XSOCKET_TYPE_DGRAM   2
#define XSOCKET_ROLE_CLIENT  0
#define XSOCKET_FAMILY_IPV4  1

#define GRPS_TIMER_DEACTIVATED -1
#define GRPS_TIMER_INVALID     -2

int ctr_lte_v2_parse_xsocket_set(const char *s, struct xsocket_set_param *param);
int ctr_lte_v2_parse_xsocket_get(const char *s, struct xsocket_get_param *param);
int ctr_lte_v2_parse_urc_cereg(const char *s, struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_parse_urc_xmodemsleep(const char *s, int *p1, int *p2);
int ctr_lte_v2_parse_coneval(const char *s, struct ctr_lte_v2_conn_param *param);
int ctr_lte_v2_parse_urc_xgps(const char *s, struct ctr_lte_v2_gnss_update *update);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_PARSE_H_ */
