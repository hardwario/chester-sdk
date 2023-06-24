/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LRW_TALK_H_
#define CHESTER_SUBSYS_CTR_LRW_TALK_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lrw_talk_event {
	CTR_LRW_TALK_EVENT_BOOT = 0,
	CTR_LRW_TALK_EVENT_JOIN_OK = 1,
	CTR_LRW_TALK_EVENT_JOIN_ERR = 2,
	CTR_LRW_TALK_EVENT_SEND_OK = 3,
	CTR_LRW_TALK_EVENT_SEND_ERR = 4,
};

typedef void (*ctr_lrw_talk_event_cb)(enum ctr_lrw_talk_event event);

int ctr_lrw_talk_init(ctr_lrw_talk_event_cb event_cb);
int ctr_lrw_talk_enable(void);
int ctr_lrw_talk_disable(void);
int ctr_lrw_talk_(const char *s);
int ctr_lrw_talk_at(void);
int ctr_lrw_talk_at_dformat(uint8_t df);
int ctr_lrw_talk_at_band(uint8_t band);
int ctr_lrw_talk_at_class(uint8_t class);
int ctr_lrw_talk_at_dr(uint8_t dr);
int ctr_lrw_talk_at_mode(uint8_t mode);
int ctr_lrw_talk_at_nwk(uint8_t network);
int ctr_lrw_talk_at_adr(uint8_t adr);
int ctr_lrw_talk_at_dutycycle(uint8_t dc);
int ctr_lrw_talk_at_joindc(uint8_t jdc);
int ctr_lrw_talk_at_join(void);
int ctr_lrw_talk_at_devaddr(const uint8_t *devaddr, size_t devaddr_size);
int ctr_lrw_talk_at_deveui(const uint8_t *deveui, size_t deveui_size);
int ctr_lrw_talk_at_appeui(const uint8_t *appeui, size_t appeui_size);
int ctr_lrw_talk_at_appkey(const uint8_t *appkey, size_t appkey_size);
int ctr_lrw_talk_at_nwkskey(const uint8_t *nwkskey, size_t nwkskey_size);
int ctr_lrw_talk_at_appskey(const uint8_t *appskey, size_t appskey_size);
int ctr_lrw_talk_at_chmask(const char *chmask);
int ctr_lrw_talk_at_utx(const void *payload, size_t payload_len);
int ctr_lrw_talk_at_ctx(const void *payload, size_t payload_len);
int ctr_lrw_talk_at_putx(uint8_t port, const void *payload, size_t payload_len);
int ctr_lrw_talk_at_pctx(uint8_t port, const void *payload, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LRW_TALK_H_ */
