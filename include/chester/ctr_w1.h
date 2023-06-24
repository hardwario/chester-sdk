/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_W1_H_
#define CHESTER_INCLUDE_CTR_W1_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_w1 {
	sys_slist_t scan_list;
	bool is_ds28e17_present;
};

int ctr_w1_acquire(struct ctr_w1 *w1, const struct device *dev);
int ctr_w1_release(struct ctr_w1 *w1, const struct device *dev);
int ctr_w1_scan(struct ctr_w1 *w1, const struct device *dev,
		int (*user_cb)(struct w1_rom rom, void *user_data), void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_W1_H_ */
