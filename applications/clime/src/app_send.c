/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_lrw.h"
#include "app_send.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

int app_send(void)
{
	__unused int ret;

#if defined(FEATURE_SUBSYSTEM_LRW)
	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

	switch (g_app_config.mode) {

#if defined(FEATURE_SUBSYSTEM_LRW)
	case APP_CONFIG_MODE_LRW:
		ret = app_lrw_encode(&lrw_buf);
		if (ret) {
			LOG_ERR("Call `app_lrw_encode` failed: %d", ret);
			return ret;
		}

		struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
		ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&lrw_buf), ctr_buf_get_used(&lrw_buf),
				   NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
			return ret;
		}
		break;
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

	default:
		break;
	}

	return 0;
}

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)

static int compose_x4_line_alert(struct ctr_buf *buf, bool line_connected_event)
{
	int ret = 0;

	ctr_buf_reset(buf);

	ret |= ctr_buf_append_u8(buf, BIT(7));
	ret |= ctr_buf_append_u8(buf, line_connected_event ? 1 : 0);

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

int app_send_lrw_x4_line_alert(bool line_connected_event)
{
	int ret;

	CTR_BUF_DEFINE_STATIC(x4_lrw_buf, 2);

	ret = compose_x4_line_alert(&x4_lrw_buf, line_connected_event);
	if (ret) {
		LOG_ERR("Call `compose_x4_line_alert` failed: %d", ret);
		return ret;
	}

	struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;

	ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&x4_lrw_buf), ctr_buf_get_used(&x4_lrw_buf),
			   NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B) */
