/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_msg.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Third-party includes */
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_cloud_msg_data, CONFIG_CTR_CLOUD_LOG_LEVEL);

/* Payload bounds of the running ctr_cloud_msg_pack_data_cb(); the zcbor state does not
 * carry the start. Only touched by that function, which callers serialize.
 */
static const uint8_t *m_payload_start;
static const uint8_t *m_payload_end;

int ctr_cloud_msg_zcbor_errno(int zcbor_err)
{
	if (zcbor_err == ZCBOR_SUCCESS) {
		return 0;
	}

	switch (zcbor_err) {
	case ZCBOR_ERR_NO_PAYLOAD:
		return -ENOSPC;
	case ZCBOR_ERR_NO_BACKUP_MEM:
		return -ENOMEM;
	default:
		return -EBADMSG;
	}
}

static int pack_data_header(struct ctr_buf *buf, uint64_t decoder_hash)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_DATA);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u64_be(buf, decoder_hash);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u64_be` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_msg_pack_data(struct ctr_buf *buf, uint64_t decoder_hash, const uint8_t *data,
			    size_t len)
{
	int ret;

	ret = pack_data_header(buf, decoder_hash);
	if (ret) {
		return ret;
	}

	ret = ctr_buf_append_mem(buf, data, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int pack_data_cb(struct ctr_buf *buf, uint64_t decoder_hash, ctr_cloud_data_encode_cb cb,
			void *user_data)
{
	int ret = pack_data_header(buf, decoder_hash);

	if (ret) {
		return ret;
	}

	size_t header_end = ctr_buf_get_used(buf);

	/* On the caller's stack, not static: no RAM cost between calls */
	ZCBOR_STATE_E(zs, CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES, ctr_buf_get_mem(buf) + header_end,
		      ctr_buf_get_free(buf), 1);

	/* zcbor_new_state() switches it off, CONFIG_ZCBOR_STOP_ON_ERROR only compiles it in */
	zs->constant_state->stop_on_error = true;

	const uint8_t *start = zs->payload_mut;

	/* A callback may (wrongly) start a nested encode, keep the outer one intact */
	const uint8_t *prev_start = m_payload_start;
	const uint8_t *prev_end = m_payload_end;

	m_payload_start = start;
	m_payload_end = zs->payload_end;
	ret = cb(zs, user_data);
	m_payload_start = prev_start;
	m_payload_end = prev_end;

	if (ret > 0) {
		/* Positive values are reserved for the cloud subsystem */
		return -EINVAL;
	}
	if (ret) {
		return ret;
	}

	int zerr = zcbor_peek_error(zs);

	if (zerr) {
		LOG_WRN("Encoding failed, zcbor error: %d", zerr);
		return ctr_cloud_msg_zcbor_errno(zerr);
	}

	size_t len = zs->payload_mut - start;

	if (len == 0) {
		return -EINVAL;
	}

	return ctr_buf_seek(buf, header_end + len);
}

int ctr_cloud_msg_pack_data_cb(struct ctr_buf *buf, uint64_t decoder_hash,
			       ctr_cloud_data_encode_cb cb, void *user_data)
{
	if (!buf || !cb) {
		return -EINVAL;
	}

	size_t used = ctr_buf_get_used(buf);
	int ret = pack_data_cb(buf, decoder_hash, cb, user_data);

	if (ret) {
		/* Leave no partial message behind */
		(void)ctr_buf_seek(buf, used);
	}

	return ret;
}

size_t ctr_cloud_msg_data_len(const zcbor_state_t *zs)
{
	if (!m_payload_start || zs->payload_end != m_payload_end ||
	    zs->payload_mut < m_payload_start || zs->payload_mut > m_payload_end) {
		return 0;
	}

	return zs->payload_mut - m_payload_start;
}
