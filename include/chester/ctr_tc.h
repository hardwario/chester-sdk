/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_TC_H_
#define CHESTER_INCLUDE_CTR_TC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_tc ctr_tc
 * @{
 */

enum ctr_tc_channel {
	CTR_TC_CHANNEL_A1 = 0,
	CTR_TC_CHANNEL_A2 = 1,
	CTR_TC_CHANNEL_B1 = 2,
	CTR_TC_CHANNEL_B2 = 3,
};

enum ctr_tc_type {
	CTR_TC_TYPE_K = 0,
};

int ctr_tc_read(enum ctr_tc_channel channel, enum ctr_tc_type type, float *temperature);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_TC_H_ */
