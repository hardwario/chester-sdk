/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lrw.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_encode.h>
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)

static int compose_lrw(struct ctr_buf *buf)
{
	int ret = 0;

	ctr_buf_reset(buf);

	app_data_lock();

	uint8_t header = 0;

	/* Flag BATT */
	if (IS_ENABLED(FEATURE_SUBSYSTEM_BATT)) {
		header |= BIT(0);
	}

	/* Flag ACCEL */
	if (IS_ENABLED(FEATURE_SUBSYSTEM_ACCEL)) {
		header |= BIT(1);
	}

	/* Flag THERM */
	if (IS_ENABLED(FEATURE_SUBSYSTEM_THERM)) {
		header |= BIT(2);
	}

	ret |= ctr_buf_append_u8(buf, header);

	/* Field BATT */
	if (header & BIT(0)) {
		if (isnan(g_app_data.system_voltage_rest)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_rest;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(g_app_data.system_voltage_load)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_load;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(g_app_data.system_current_load)) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			uint8_t val = g_app_data.system_current_load;
			ret |= ctr_buf_append_u8(buf, val);
		}
	}

	/* Field ACCEL */
	if (header & BIT(1)) {
		if (g_app_data.accel_orientation == INT_MAX) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			uint8_t val = g_app_data.accel_orientation;
			ret |= ctr_buf_append_u8(buf, val);
		}
	}

	/* Field THERM */
	if (header & BIT(2)) {
		if (isnan(g_app_data.therm_temperature)) {
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
		} else {
			int16_t val = g_app_data.therm_temperature * 100.f;
			ret |= ctr_buf_append_s16_le(buf, val);
		}
	}

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

int app_send(void)
{
#if defined(FEATURE_SUBSYSTEM_LRW) || defined(FEATURE_SUBSYSTEM_LTE_V2)
	int ret;
#endif /* defined(FEATURE_SUBSYSTEM_LRW) || defined(FEATURE_SUBSYSTEM_LTE_V2)*/

	switch (g_app_config.mode) {
#if defined(FEATURE_SUBSYSTEM_LRW)
	case APP_CONFIG_MODE_LRW:
		CTR_BUF_DEFINE_STATIC(lrw_buf, 51);

		ret = compose_lrw(&lrw_buf);
		if (ret) {
			LOG_ERR("Call `compose_lrw` failed: %d", ret);
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

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
	case APP_CONFIG_MODE_LTE:
		CTR_BUF_DEFINE_STATIC(buf, 8 * 1024);

		ctr_buf_reset(&buf);

		ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

		ret = app_cbor_encode(zs);
		if (ret) {
			LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
			return ret;
		}

		size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

		ret = ctr_buf_seek(&buf, len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return ret;
		}

		ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
		if (ret) {
			LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
			return ret;
		}
		break;
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

	default:
		break;
	}

	return 0;
}