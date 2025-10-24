/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_DATA_H_
#define CHESTER_INCLUDE_CTR_DATA_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_data ctr_data
 * @{
 */

struct ctr_data_aggreg {
	float min;
	float max;
	float avg;
	float mdn;
};

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_DATA_H_ */
