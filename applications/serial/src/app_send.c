/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_lrw.h"
#include "app_send.h"
#include "feature.h"

/* Driver includes for sample buffers */
#include "drivers/drv_em1xx.h"
#include "drivers/drv_em5xx.h"
#include "drivers/drv_iem3000.h"
#include "drivers/drv_microsens_180hs.h"
#include "drivers/drv_or_we_504.h"
#include "drivers/drv_or_we_516.h"
#include "drivers/drv_promag_mf7s.h"
#include "drivers/drv_flowt_ft201.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

#if FEATURE_SUBSYSTEM_LRW
#include <chester/ctr_lrw.h>
#endif

#if FEATURE_SUBSYSTEM_CLOUD
#include <chester/ctr_cloud.h>
#include "app_cbor.h"
#include <zcbor_common.h>
#include <zcbor_encode.h>
#endif

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

int app_send(void)
{
	__unused int ret;

#if FEATURE_SUBSYSTEM_LRW
	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
#endif /* FEATURE_SUBSYSTEM_LRW */

	switch (g_app_config.mode) {

	case APP_CONFIG_MODE_NONE:
		LOG_WRN("Mode is NONE, not sending data");
		break;

#if FEATURE_SUBSYSTEM_LRW
	case APP_CONFIG_MODE_LRW: {
		struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
		int sent_count = 0;

		/* Send message for each active device */
		for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
			if (g_app_config.devices[i].type == APP_DEVICE_TYPE_NONE) {
				continue;
			}

			/* Encode single device */
			ret = app_lrw_encode_single(&lrw_buf, i);
			if (ret) {
				LOG_ERR("Failed to encode device %d: %d", i, ret);
				continue; /* Skip this device, try next */
			}

			/* Send message */
			ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&lrw_buf),
					   ctr_buf_get_used(&lrw_buf), NULL);
			if (ret) {
				LOG_ERR("Failed to send device %d: %d", i, ret);
				continue; /* Skip this device, try next */
			}

			LOG_INF("Sent message for device %d (%zu bytes)", i,
				ctr_buf_get_used(&lrw_buf));
			sent_count++;

			/* Check if there are more devices to send */
			bool has_more = false;
			for (int j = i + 1; j < APP_CONFIG_MAX_DEVICES; j++) {
				if (g_app_config.devices[j].type != APP_DEVICE_TYPE_NONE) {
					has_more = true;
					break;
				}
			}

			/* Wait 1 second before next message */
			if (has_more) {
				LOG_DBG("Waiting 1s before next message...");
				k_sleep(K_SECONDS(1));
			}
		}

		if (sent_count == 0) {
			LOG_WRN("No devices to send");
			return -ENODEV;
		}

		LOG_INF("Sent %d message(s) total", sent_count);
		break;
	}
#endif /* FEATURE_SUBSYSTEM_LRW */

#if FEATURE_SUBSYSTEM_CLOUD
	case APP_CONFIG_MODE_LTE: {
		/* 8KB buffer for CBOR payload */
		CTR_BUF_DEFINE_STATIC(buf, 8 * 1024);

		ctr_buf_reset(&buf);

		/* Initialize ZCBOR encoder state */
		ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

		ret = app_cbor_encode(zs);
		if (ret) {
			LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
			return ret;
		}

		/* Calculate encoded data length */
		size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

		ret = ctr_buf_seek(&buf, len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sending %zu bytes via LTE", ctr_buf_get_used(&buf));

		ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
		if (ret) {
			LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
			return ret;
		}

		/* Clear sample buffers after successful upload */
		microsens_clear_samples();
		or_we_504_clear_samples();
		or_we_516_clear_samples();
		em1xx_clear_samples();
		em5xx_clear_samples();
		iem3000_clear_samples();
		promag_mf7s_clear_samples();
		flowt_ft201_clear_samples();

		break;
	}
#endif /* FEATURE_SUBSYSTEM_CLOUD */

	default:
		break;
	}

	return 0;
}
