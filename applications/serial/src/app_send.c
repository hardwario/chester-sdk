/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_device.h"
#include "app_driver_store.h"
#include "app_lrw.h"
#include "app_send.h"
#include "app_send_split.h"
#include "app_sensor.h"
#include "feature.h"

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
#include <string.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

static void clear_ble_tags(void)
{
#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	int ret = app_sensor_ble_tag_clear();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_clear` failed: %d", ret);
	}
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
}

#if FEATURE_SUBSYSTEM_CLOUD

/*
 * A report carries the records and windows of every device and the BLE tag measurements
 * collected since the previous report. Like the other applications, Serial does not retry:
 * once a report has been handed to ctr_cloud, what it selected is dropped whether the send
 * succeeded or not. Delivery after a failure is the job of the cloud spool
 * (CONFIG_CTR_CLOUD_SPOOL). A report larger than the payload buffer is sent in several
 * messages (app_send_split.h).
 */

#define CBOR_BUF_SIZE    (8 * 1024)
#define CBOR_STATE_COUNT 8

BUILD_ASSERT(APP_SEND_SLOTS == APP_CONFIG_MAX_DEVICES);

static uint8_t m_cbor_buf[CBOR_BUF_SIZE];
static size_t m_cbor_len;
static uint32_t m_sequence;

/* Everything collected so far */
static void select_all(struct app_send_select *sel)
{
	memset(sel, 0, sizeof(*sel));

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = app_device_get_slot(i);

		if (!slot) {
			continue;
		}

		app_driver_slot_lock(slot);
		sel->rec[i].first = app_driver_rec_first(slot);
		sel->rec[i].end = app_driver_rec_end(slot);
		sel->win[i].first = app_driver_win_first(slot);
		sel->win[i].end = app_driver_win_end(slot);
		app_driver_slot_unlock(slot);
	}

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	sel->ble_tags = true;
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
}

/* Encode a message into m_cbor_buf; -ENOSPC if it does not fit */
static int encode(struct app_send_select *sel, void *user)
{
	int ret;
	zcbor_state_t zs[CBOR_STATE_COUNT + 2];

	ARG_UNUSED(user);

	zcbor_new_encode_state(zs, ARRAY_SIZE(zs), m_cbor_buf, sizeof(m_cbor_buf), 1);

	sel->sequence = m_sequence;

	ret = app_cbor_encode(zs, sel);
	if (ret) {
		return ret;
	}

	m_cbor_len = zs[0].payload_mut - m_cbor_buf;

	return 0;
}

/* Drop what a sent message carried */
static void drop_sent(const struct app_send_select *sel)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = app_device_get_slot(i);

		if (!slot) {
			continue;
		}

		app_driver_slot_lock(slot);

		if (!app_send_range_empty(&sel->rec[i])) {
			app_driver_rec_pop_until(slot, sel->rec[i].end);
		}

		if (!app_send_range_empty(&sel->win[i])) {
			app_driver_win_pop_until(slot, sel->win[i].end);
		}

		app_driver_slot_unlock(slot);
	}

	if (sel->ble_tags) {
		clear_ble_tags();
	}
}

static int send_message(struct app_send_select *sel, void *user)
{
	int ret;

	ret = encode(sel, user);
	if (ret) {
		return ret;
	}

	LOG_INF("Sending %zu bytes via LTE (sequence %u)", m_cbor_len, sel->sequence);

	/* Every attempt gets its own sequence number: a failed message may still be delivered
	 * later from the cloud spool
	 */
	m_sequence++;

	ret = ctr_cloud_send(m_cbor_buf, m_cbor_len);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
		return ret;
	}

	drop_sent(sel);

	return 0;
}

static void drop_oversized(int idx, bool windows, uint32_t seq, void *user)
{
	struct app_driver_slot *slot = app_device_get_slot(idx);

	ARG_UNUSED(user);

	LOG_ERR("Device[%d]: %s %u does not fit a message, dropped", idx,
		windows ? "window" : "record", seq);

	if (!slot) {
		return;
	}

	app_driver_slot_lock(slot);

	if (windows) {
		app_driver_win_pop_until(slot, seq + 1);
	} else {
		app_driver_rec_pop_until(slot, seq + 1);
	}

	app_driver_slot_unlock(slot);
}

static void drop_ble_tags(void *user)
{
	ARG_UNUSED(user);

	LOG_ERR("BLE tag measurements do not fit a message, dropped");
	clear_ble_tags();
}

static const struct app_send_split_ops m_split_ops = {
	.encode = encode,
	.send = send_message,
	.drop = drop_oversized,
	.drop_ble_tags = drop_ble_tags,
};

static int send_lte(void)
{
	int ret;
	struct app_send_select pending;

	struct app_send_select report;

	select_all(&pending);
	report = pending;

	ret = send_message(&pending, NULL);
	if (ret == -ENOSPC) {
		LOG_WRN("Report exceeds %u B, sending it in several messages", CBOR_BUF_SIZE);

		ret = app_send_split(&pending, &m_split_ops, NULL);
	}

	/* No retry: drop the whole report, sent or not (app_send_split() modifies pending) */
	drop_sent(&report);

	return ret;
}

#endif /* FEATURE_SUBSYSTEM_CLOUD */

#if FEATURE_SUBSYSTEM_LRW

/* A LoRaWAN uplink carries the latest values: drop the records and the windows but the newest
 * closed one of every device in it, which later uplinks send until the next window closes
 */
static void drop_sent_lrw(uint8_t packed_mask)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = app_device_get_slot(i);

		if (!(packed_mask & BIT(i)) || !slot) {
			continue;
		}

		app_driver_slot_lock(slot);
		app_driver_rec_pop_until(slot, app_driver_rec_end(slot));
		app_driver_win_pop_until(slot, app_driver_win_end(slot) - 1);
		app_driver_slot_unlock(slot);
	}
}

#endif /* FEATURE_SUBSYSTEM_LRW */

int app_send(void)
{
	__unused int ret;

#if FEATURE_SUBSYSTEM_LRW
	/* Deliberately larger than APP_LRW_MTU: app_lrw_encode_multi() encodes a device and
	 * rolls it back when it would exceed the MTU, so it needs room to overshoot into.
	 */
	CTR_BUF_DEFINE_STATIC(lrw_buf, 96);
#endif /* FEATURE_SUBSYSTEM_LRW */

	switch (g_app_config.mode) {

	case APP_CONFIG_MODE_NONE:
		LOG_WRN("Mode is NONE, not sending data");

		/* Nothing is reported: keep the buffers from filling up */
		app_device_clear_all();
		clear_ble_tags();
		break;

#if FEATURE_SUBSYSTEM_LRW
	case APP_CONFIG_MODE_LRW: {
		struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
		int sent_count = 0;

		/* One message per MTU-worth of devices, not one per device: three RPP-R probes
		 * fit in 45 of 51 bytes, so a three-probe installation goes from three
		 * transmissions to one. Only a set that genuinely cannot fit is split, and then
		 * we resume from wherever the encoder stopped.
		 */
		int idx = 0;

		while (idx < APP_CONFIG_MAX_DEVICES) {
			int next_idx = APP_CONFIG_MAX_DEVICES;
			uint8_t packed_mask;

			ret = app_lrw_encode_multi(&lrw_buf, idx, &next_idx, &packed_mask);
			if (ret < 0) {
				LOG_ERR("Failed to encode from device %d: %d", idx, ret);
				if (next_idx <= idx) {
					break; /* no forward progress - do not spin */
				}
				idx = next_idx;
				continue;
			}

			int packed = ret;

			if (packed == 0) {
				break; /* nothing left to send */
			}

			ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&lrw_buf),
					   ctr_buf_get_used(&lrw_buf), NULL);
			if (ret) {
				LOG_ERR("Failed to send %d device(s) from index %d: %d", packed,
					idx, ret);
			} else {
				LOG_INF("Sent message with %d device(s) (%zu bytes)", packed,
					ctr_buf_get_used(&lrw_buf));
				sent_count++;
				drop_sent_lrw(packed_mask);
			}

			if (next_idx <= idx) {
				break; /* defensive: always make progress */
			}

			idx = next_idx;

			if (idx < APP_CONFIG_MAX_DEVICES) {
				LOG_DBG("Waiting 1s before the next message...");
				k_sleep(K_SECONDS(1));
			}
		}

		if (sent_count == 0) {
			LOG_WRN("No devices to send");
			return -ENODEV;
		}

		clear_ble_tags();

		LOG_INF("Sent %d message(s) total", sent_count);
		break;
	}
#endif /* FEATURE_SUBSYSTEM_LRW */

#if FEATURE_SUBSYSTEM_CLOUD
	case APP_CONFIG_MODE_LTE:
		ret = send_lte();
		if (ret) {
			LOG_ERR("Call `send_lte` failed: %d", ret);
			return ret;
		}
		break;
#endif /* FEATURE_SUBSYSTEM_CLOUD */

	default:
		break;
	}

	return 0;
}
