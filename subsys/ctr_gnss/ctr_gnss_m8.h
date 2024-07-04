/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_GNSS_M8_H_
#define CHESTER_SUBSYS_CTR_GNSS_M8_H_

/* CHESTER includes */
#include <chester/ctr_gnss.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_gnss_m8_start(void);
int ctr_gnss_m8_stop(bool keep_bckp_domain);
int ctr_gnss_m8_set_handler(ctr_gnss_user_cb user_cb, void *user_data);
int ctr_gnss_m8_process_data(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_GNSS_M8_H_ */
