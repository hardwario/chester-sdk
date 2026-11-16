/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_msg.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>

/* Third-party includes */
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

size_t ctr_cloud_cbor_free(const zcbor_state_t *zs)
{
	return zs->payload_end - zs->payload_mut;
}

size_t ctr_cloud_cbor_len(const zcbor_state_t *zs)
{
	return ctr_cloud_msg_data_len(zs);
}

int ctr_cloud_cbor_put_raw(zcbor_state_t *zs, const void *cbor, size_t len)
{
	if (!zs) {
		return -EINVAL;
	}

	if (!cbor || !len) {
		/* Also fail the state, so that ignoring the return value cannot send a bad message
		 */
		zcbor_error(zs, ZCBOR_ERR_WRONG_VALUE);
		return -EINVAL;
	}

	/* Same as zcbor with stop_on_error: nothing is written after the first error */
	int err = zcbor_peek_error(zs);

	if (err) {
		return ctr_cloud_msg_zcbor_errno(err);
	}

	if (len > ctr_cloud_cbor_free(zs)) {
		zcbor_error(zs, ZCBOR_ERR_NO_PAYLOAD);
		return -ENOSPC;
	}

	memcpy(zs->payload_mut, cbor, len);
	zs->payload_mut += len;
	zs->elem_count++;

	return 0;
}

void ctr_cloud_cbor_put_key_ts(zcbor_state_t *zs, uint32_t key, int64_t ts)
{
	zcbor_uint32_put(zs, key);

	if (ts == 0) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int64_put(zs, ts);
	}
}
