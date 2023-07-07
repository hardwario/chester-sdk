/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_TALK_H_
#define CHESTER_SUBSYS_CTR_LTE_TALK_H_

#include <chester/ctr_lte.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_talk_event {
	CTR_LTE_TALK_EVENT_BOOT = 0,
	CTR_LTE_TALK_EVENT_SIM_CARD = 1,
	CTR_LTE_TALK_EVENT_TIME = 2,
	CTR_LTE_TALK_EVENT_ATTACH = 3,
	CTR_LTE_TALK_EVENT_DETACH = 4,
};

typedef void (*ctr_lte_talk_event_cb)(enum ctr_lte_talk_event event);

int ctr_lte_talk_init(ctr_lte_talk_event_cb event_cb);
int ctr_lte_talk_enable(void);
int ctr_lte_talk_disable(void);
int ctr_lte_talk_(const char *s);
int ctr_lte_talk_at(void);
int ctr_lte_talk_at_cclk_q(char *buf, size_t size);
int ctr_lte_talk_at_ceppi(int p1);
int ctr_lte_talk_at_cimi(char *buf, size_t size);
int ctr_lte_talk_at_cereg(int p1);
int ctr_lte_talk_at_cfun(int p1);
int ctr_lte_talk_at_cgauth(int p1, int *p2, const char *p3, const char *p4);
int ctr_lte_talk_at_cgdcont(int p1, const char *p2, const char *p3);
int ctr_lte_talk_at_cgerep(int p1);
int ctr_lte_talk_at_cgsn(char *buf, size_t size);
int ctr_lte_talk_at_cmee(int p1);
int ctr_lte_talk_at_cnec(int p1);
int ctr_lte_talk_at_coneval(struct ctr_lte_eval *eval);
int ctr_lte_talk_at_cops(int p1, int *p2, const char *p3);
int ctr_lte_talk_at_cpsms(int *p1, const char *p2, const char *p3);
int ctr_lte_talk_at_crsm_176(char *buf, size_t size);
int ctr_lte_talk_at_crsm_214(void);
int ctr_lte_talk_at_cscon(int p1);
int ctr_lte_talk_at_hwversion(char *buf, size_t size);
int ctr_lte_talk_at_mdmev(int p1);
int ctr_lte_talk_at_rai(int p1);
int ctr_lte_talk_at_rel14feat(int p1, int p2, int p3, int p4, int p5);
int ctr_lte_talk_at_shortswver(char *buf, size_t size);
int ctr_lte_talk_at_xbandlock(int p1, const char *p2);
int ctr_lte_talk_at_xdataprfl(int p1);
int ctr_lte_talk_at_xnettime(int p1, int *p2);
int ctr_lte_talk_at_xpofwarn(int p1, int p2);
int ctr_lte_talk_at_xsendto(const char *p1, int p2, const void *buf, size_t len);
int ctr_lte_talk_at_xsocketopt(int p1, int p2, int *p3);
int ctr_lte_talk_at_xsim(int p1);
int ctr_lte_talk_at_xsleep(int p1);
int ctr_lte_talk_at_xsocket(int p1, int *p2, int *p3, char *buf, size_t size);
int ctr_lte_talk_at_xsystemmode(int p1, int p2, int p3, int p4);
int ctr_lte_talk_at_xtime(int p1);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_TALK_H_ */
