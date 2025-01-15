/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_SPS30_H_
#define CHESTER_INCLUDE_CTR_SPS30_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_sps30 ctr_sps30
 * @{
 */
int ctr_sps30_read(float *mass_conc_pm_1_0, float *mass_conc_pm_2_5, float *mass_conc_pm_4_0,
		   float *mass_conc_pm_10_0, float *num_conc_pm_0_5, float *num_conc_pm_1_0,
		   float *num_conc_pm_2_5, float *num_conc_pm_4_0, float *num_conc_pm_10_0);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_SPS30_H_ */
