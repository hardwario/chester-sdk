/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LRW_V2_CTR_LRW_TALK_H_
#define CHESTER_SUBSYS_CTR_LRW_V2_CTR_LRW_TALK_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_lrw_v2_talk;

typedef void (*ctr_lrw_v2_talk_cb)(struct ctr_lrw_v2_talk *talk, const char *line, void *user_data);

struct ctr_lrw_v2_talk {
	const struct device *dev;
	ctr_lrw_v2_talk_cb user_cb;
	void *user_data;
};

int ctr_lrw_v2_talk_init(struct ctr_lrw_v2_talk *talk, const struct device *dev);
int ctr_lrw_v2_talk_set_callback(struct ctr_lrw_v2_talk *talk, ctr_lrw_v2_talk_cb user_cb,
				 void *user_data);
int ctr_lrw_v2_talk_(struct ctr_lrw_v2_talk *talk, const char *s);
int ctr_lrw_v2_talk_at(struct ctr_lrw_v2_talk *talk);
int ctr_lrw_v2_talk_at_dformat(struct ctr_lrw_v2_talk *talk, uint8_t df);
int ctr_lrw_v2_talk_at_band(struct ctr_lrw_v2_talk *talk, uint8_t band);
int ctr_lrw_v2_talk_at_class(struct ctr_lrw_v2_talk *talk, uint8_t class);
int ctr_lrw_v2_talk_at_dr(struct ctr_lrw_v2_talk *talk, uint8_t dr);
int ctr_lrw_v2_talk_at_mode(struct ctr_lrw_v2_talk *talk, uint8_t mode);
int ctr_lrw_v2_talk_at_nwk(struct ctr_lrw_v2_talk *talk, uint8_t network);
int ctr_lrw_v2_talk_at_adr(struct ctr_lrw_v2_talk *talk, uint8_t adr);
int ctr_lrw_v2_talk_at_dutycycle(struct ctr_lrw_v2_talk *talk, uint8_t dc);
int ctr_lrw_v2_talk_at_async(struct ctr_lrw_v2_talk *talk, bool async);
int ctr_lrw_v2_talk_at_joindc(struct ctr_lrw_v2_talk *talk, uint8_t jdc);
int ctr_lrw_v2_talk_at_join(struct ctr_lrw_v2_talk *talk);
int ctr_lrw_v2_talk_at_devaddr(struct ctr_lrw_v2_talk *talk, const uint8_t *devaddr,
			       size_t devaddr_size);
int ctr_lrw_v2_talk_at_deveui(struct ctr_lrw_v2_talk *talk, const uint8_t *deveui,
			      size_t deveui_size);
int ctr_lrw_v2_talk_at_appeui(struct ctr_lrw_v2_talk *talk, const uint8_t *appeui,
			      size_t appeui_size);
int ctr_lrw_v2_talk_at_appkey(struct ctr_lrw_v2_talk *talk, const uint8_t *appkey,
			      size_t appkey_size);
int ctr_lrw_v2_talk_at_nwkskey(struct ctr_lrw_v2_talk *talk, const uint8_t *nwkskey,
			       size_t nwkskey_size);
int ctr_lrw_v2_talk_at_appskey(struct ctr_lrw_v2_talk *talk, const uint8_t *appskey,
			       size_t appskey_size);
int ctr_lrw_v2_talk_at_chmask(struct ctr_lrw_v2_talk *talk, const char *chmask);
int ctr_lrw_v2_talk_at_utx(struct ctr_lrw_v2_talk *talk, const void *payload, size_t payload_len);
int ctr_lrw_v2_talk_at_ctx(struct ctr_lrw_v2_talk *talk, const void *payload, size_t payload_len);
int ctr_lrw_v2_talk_at_putx(struct ctr_lrw_v2_talk *talk, uint8_t port, const void *payload,
			    size_t payload_len);
int ctr_lrw_v2_talk_at_pctx(struct ctr_lrw_v2_talk *talk, uint8_t port, const void *payload,
			    size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LRW_V2_CTR_LRW_TALK_H_ */
