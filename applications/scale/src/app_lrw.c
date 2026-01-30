/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_lrw.h"
#include "app_config.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_encode_lrw.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_lrw, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)

int app_lrw_encode(struct ctr_buf *buf)
{
	int ret = 0;

	ctr_buf_reset(buf);

	app_data_lock();

	/* Build header byte */
	uint8_t header = 0;

	header |= IS_ENABLED(CONFIG_CTR_BATT) ? BIT(0) : 0;
	header |= IS_ENABLED(CONFIG_CTR_ACCEL) ? BIT(1) : 0;
	header |= IS_ENABLED(CONFIG_CTR_THERM) ? BIT(2) : 0;
	/* BIT(3-5) reserved */
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_X3_A) ? BIT(6) : 0;

	ret |= ctr_buf_append_u8(buf, header);

#if defined(CONFIG_CTR_BATT)
	CTR_ENCODE_LRW_BATTERY(buf);
#endif

#if defined(CONFIG_CTR_ACCEL)
	CTR_ENCODE_LRW_ACCEL(buf);
#endif

#if defined(CONFIG_CTR_THERM)
	CTR_ENCODE_LRW_THERM(buf);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	/* Channel active mask (bit 0-3 = A1, A2, B1, B2) */
	uint8_t channel_mask = 0;
	channel_mask |= g_app_config.channel_a1_active ? BIT(0) : 0;
	channel_mask |= g_app_config.channel_a2_active ? BIT(1) : 0;
	channel_mask |= g_app_config.channel_b1_active ? BIT(2) : 0;
	channel_mask |= g_app_config.channel_b2_active ? BIT(3) : 0;
	ret |= ctr_buf_append_u8(buf, channel_mask);

	/* Get latest measurement if available */
	int32_t a1 = INT32_MAX, a2 = INT32_MAX, b1 = INT32_MAX, b2 = INT32_MAX;
	if (g_app_data.weight_measurement_count > 0) {
		int idx = g_app_data.weight_measurement_count - 1;
		a1 = g_app_data.weight_measurements[idx].a1_raw;
		a2 = g_app_data.weight_measurements[idx].a2_raw;
		b1 = g_app_data.weight_measurements[idx].b1_raw;
		b2 = g_app_data.weight_measurements[idx].b2_raw;
	}

	ret |= ctr_buf_append_s32_le(buf, a1);
	ret |= ctr_buf_append_s32_le(buf, a2);
	ret |= ctr_buf_append_s32_le(buf, b1);
	ret |= ctr_buf_append_s32_le(buf, b2);
#endif

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_LRW) */
