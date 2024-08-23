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
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_encode.h>

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
	if (IS_ENABLED(CONFIG_CTR_BATT)) {
		header |= BIT(0);
	}

	/* Flag ACCEL */
	if (IS_ENABLED(CONFIG_CTR_ACCEL)) {
		header |= BIT(1);
	}

	/* Flag THERM */
	if (IS_ENABLED(CONFIG_CTR_THERM)) {
		header |= BIT(2);
	}

	/* FLAG BACKUP */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_Z)) {
		header |= BIT(3);
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

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	/* Field BACKUP */
	if (header & BIT(3)) {
		struct app_data_backup *backup = &g_app_data.backup;

		if (isnan(backup->line_voltage)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			ret |= ctr_buf_append_u16_le(buf, backup->line_voltage * 1000.0f);
		}

		if (isnan(backup->battery_voltage)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			ret |= ctr_buf_append_u16_le(buf, backup->battery_voltage * 1000.0f);
		}

		ret |= ctr_buf_append_u8(buf, backup->line_present ? 1 : 0);
	}

	uint16_t button_events = 0;

	for (size_t i = 0; i < APP_DATA_BUTTON_COUNT; i++) {
		ret |= ctr_buf_append_u16_le(buf, g_app_data.button[i].click_count);
		ret |= ctr_buf_append_u16_le(buf, g_app_data.button[i].hold_count);

		for (size_t j = 0; j < g_app_data.button[i].event_count; j++) {
			button_events |= BIT(2 * i) << g_app_data.button[i].events[j].type;
		}
	}

	ret |= ctr_buf_append_u16_le(buf, button_events);
#endif

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

int app_send(void)
{
	int ret;

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
