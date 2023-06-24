/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_RTD_H_
#define CHESTER_INCLUDE_CTR_RTD_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_rtd_channel {
	CTR_RTD_CHANNEL_A1 = 0,
	CTR_RTD_CHANNEL_A2 = 1,
	CTR_RTD_CHANNEL_B1 = 2,
	CTR_RTD_CHANNEL_B2 = 3,
};

enum ctr_rtd_type {
	CTR_RTD_TYPE_PT100 = 0,
	CTR_RTD_TYPE_PT1000 = 1,
};

int ctr_rtd_read(enum ctr_rtd_channel channel, enum ctr_rtd_type type, float *temperature);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_RTD_H_ */
