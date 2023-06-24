/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_WDOG_H_
#define CHESTER_INCLUDE_CTR_WDOG_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_wdog_channel {
	int id;
};

#if defined(CONFIG_CTR_WDOG)

int ctr_wdog_set_timeout(int timeout_msec);
int ctr_wdog_install(struct ctr_wdog_channel *channel);
int ctr_wdog_start(void);
int ctr_wdog_feed(struct ctr_wdog_channel *channel);

#else

static inline int ctr_wdog_set_timeout(int timeout_msec)
{
	return 0;
}

static inline int ctr_wdog_install(struct ctr_wdog_channel *channel)
{
	return 0;
}

static inline int ctr_wdog_start(void)
{
	return 0;
}

static inline int ctr_wdog_feed(struct ctr_wdog_channel *channel)
{
	return 0;
}

#endif /* defined(CONFIG_CTR_WDOG) */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_WDOG_H_ */
