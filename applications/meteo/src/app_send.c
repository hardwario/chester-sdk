/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_send.h"
#include "app_data.h"
#include "app_lrw.h"
#include "app_sensor.h"
#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)
#include "app_modbus.h"
#endif
#include "feature.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Chester includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#if defined(FEATURE_SUBSYSTEM_LRW)
#include <chester/ctr_lrw.h>
#endif

/* Standard includes */
#include <math.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

int app_send(void)
{
	int ret;

	switch (g_app_config.mode) {
	case APP_CONFIG_MODE_NONE:
		LOG_WRN("Application mode is set to none. Not sending data.");
		break;
#if defined(FEATURE_SUBSYSTEM_LRW)
	case APP_CONFIG_MODE_LRW:

		CTR_BUF_DEFINE_STATIC(lrw_buf, 51);

		ret = app_lrw_encode(&lrw_buf);
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
#if defined(FEATURE_SUBSYSTEM_CLOUD)
	case APP_CONFIG_MODE_LTE: {
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
	}
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */
	}

	return 0;
}
