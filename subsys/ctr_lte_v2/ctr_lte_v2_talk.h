/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_TALK_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_TALK_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_lte_v2_talk;

typedef void (*ctr_lte_v2_talk_cb)(struct ctr_lte_v2_talk *talk, const char *line, void *user_data);

struct ctr_lte_v2_talk {
	const struct device *dev;
	ctr_lte_v2_talk_cb user_cb;
	void *user_data;
};

int ctr_lte_v2_talk_init(struct ctr_lte_v2_talk *talk, const struct device *dev);
int ctr_lte_v2_talk_set_callback(struct ctr_lte_v2_talk *talk, ctr_lte_v2_talk_cb user_cb,
				 void *user_data);
int ctr_lte_v2_talk_(struct ctr_lte_v2_talk *talk, const char *s);
int ctr_lte_v2_talk_at_cclk_q(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_ceppi(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_cereg(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_cfun(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_cgauth(struct ctr_lte_v2_talk *talk, int p1, int *p2, const char *p3,
			      const char *p4);
int ctr_lte_v2_talk_at_cgdcont(struct ctr_lte_v2_talk *talk, int p1, const char *p2,
			       const char *p3);
int ctr_lte_v2_talk_at_cgerep(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_cgsn(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_cimi(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_iccid(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_cmee(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_cnec(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_coneval(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_cops_q(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_cops(struct ctr_lte_v2_talk *talk, int p1, int *p2, const char *p3);
int ctr_lte_v2_talk_at_cpsms(struct ctr_lte_v2_talk *talk, int *p1, const char *p2, const char *p3);
int ctr_lte_v2_talk_at_cscon(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_hwversion(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_mdmev(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_rai(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_rel14feat(struct ctr_lte_v2_talk *talk, int p1, int p2, int p3, int p4,
				 int p5);
int ctr_lte_v2_talk_at_shortswver(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_xbandlock(struct ctr_lte_v2_talk *talk, int p1, const char *p2);
int ctr_lte_v2_talk_at_xdatactrl(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xdataprfl(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xmodemsleep(struct ctr_lte_v2_talk *talk, int p1, int *p2, int *p3);
int ctr_lte_v2_talk_at_xnettime(struct ctr_lte_v2_talk *talk, int p1, int *p2);
int ctr_lte_v2_talk_at_xpofwarn(struct ctr_lte_v2_talk *talk, int p1, int p2);
int ctr_lte_v2_talk_at_xrecvfrom(struct ctr_lte_v2_talk *talk, int p1, char *buf, size_t size,
				 size_t *len);
int ctr_lte_v2_talk_at_xsendto(struct ctr_lte_v2_talk *talk, const char *p1, int p2,
			       const void *buf, size_t len);
int ctr_lte_v2_talk_at_xsend(struct ctr_lte_v2_talk *talk, const void *buf, size_t len);
int ctr_lte_v2_talk_at_xrecv(struct ctr_lte_v2_talk *talk, int timeout, char *buf, size_t size,
			     size_t *len);
int ctr_lte_v2_talk_at_xsim(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xsleep(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xslmver(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_xsocket(struct ctr_lte_v2_talk *talk, int p1, int *p2, int *p3, char *buf,
			       size_t size);
int ctr_lte_v2_talk_at_xsocketopt(struct ctr_lte_v2_talk *talk, int p1, int p2, int *p3);
int ctr_lte_v2_talk_at_xsystemmode(struct ctr_lte_v2_talk *talk, int p1, int p2, int p3, int p4);
int ctr_lte_v2_talk_at_xtemp(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xtemphighlvl(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xtime(struct ctr_lte_v2_talk *talk, int p1);
int ctr_lte_v2_talk_at_xversion(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_at_xmodemtrace(struct ctr_lte_v2_talk *talk);
int ctr_lte_v2_talk_at(struct ctr_lte_v2_talk *talk);
int ctr_lte_v2_talk_crsm_176(struct ctr_lte_v2_talk *talk, char *buf, size_t size);
int ctr_lte_v2_talk_crsm_214(struct ctr_lte_v2_talk *talk);
int ctr_lte_v2_talk_at_cmd(struct ctr_lte_v2_talk *talk, const char *s);
int ctr_lte_v2_talk_at_cmd_with_resp(struct ctr_lte_v2_talk *talk, const char *s, char *buf,
				     size_t size);
int ctr_lte_v2_talk_at_cmd_with_resp_prefix(struct ctr_lte_v2_talk *talk, const char *s, char *buf,
					    size_t size, const char *pfx);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_TALK_H_ */
