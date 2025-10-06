/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_STR_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_STR_H_

#include "ctr_lte_v2_flow.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Convert event code to string. */
const char *ctr_lte_v2_str_fsm_event(enum ctr_lte_v2_event event);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_STR_H_ */
