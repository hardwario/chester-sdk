/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * The confirmation of the running image and the ack of a firmware update done by the init
 * work (scenario TEST_FW, see mock_fw_setup()), and the stored update id.
 */

#include "common.h"
#include "ctr_cloud_msg.h"
#include "ctr_cloud_util.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <string.h>

ZTEST_SUITE(ctr_cloud_send_3_fw, NULL, common_setup_initialized, common_before, common_after, NULL);

#if TEST_FW == 1 || TEST_FW == 2
static bool contains(const uint8_t *mem, size_t len, const uint8_t *what, size_t what_len)
{
	for (size_t i = 0; i + what_len <= len; i++) {
		if (!memcmp(&mem[i], what, what_len)) {
			return true;
		}
	}

	return false;
}
#endif

ZTEST(ctr_cloud_send_3_fw, test_firmware_update_ack)
{
	uint8_t id[16];
	int session = mock_transfer_log_find(UL_CREATE_SESSION);
	int ack = mock_transfer_log_find(UL_UPLOAD_FIRMWARE);
	int config = mock_transfer_log_find(UL_UPLOAD_CONFIG);

	zassert_true(session >= 0);
	zassert_true(config > session);

#if TEST_FW == 0
	/* No update id stored (an update over BLE, or the id got lost): the image is confirmed,
	 * but there is nothing to ack (no ack with an uninitialized id)
	 */
	zassert_equal(ack, -1, "ack at %d", ack);
	zassert_equal(mock_boot_confirm_calls(), 1);
	zassert_equal(ctr_cloud_util_get_firmware_update_id(id), -ENOENT);
#elif TEST_FW == 3
	/* An update id stored, but the image was confirmed before (MCUboot reverted the
	 * update): no ack, and the id is dropped
	 */
	zassert_equal(ack, -1, "ack at %d", ack);
	zassert_equal(mock_boot_confirm_calls(), 0);
	zassert_equal(ctr_cloud_util_get_firmware_update_id(id), -ENOENT);
#else
	/* Confirmed and acked right after the session, before the uploads */
	zassert_equal(ack, session + 1, "ack at %d, session at %d", ack, session);
	zassert_equal(mock_boot_confirm_calls(), 1);

	const struct mock_log_entry *e = mock_transfer_log(ack);

	zassert_true(contains(e->mem, MIN(e->len, sizeof(e->mem)), TEST_FW_UPDATE_ID,
			      sizeof(TEST_FW_UPDATE_ID)),
		     "ack without the stored update id");

#if TEST_FW == 1
	zassert_equal(e->ret, 0);
	/* Acked: the id is dropped */
	zassert_equal(ctr_cloud_util_get_firmware_update_id(id), -ENOENT);
#else
	zassert_equal(e->ret, -EIO);
	/* Not acked: the id stays */
	zassert_ok(ctr_cloud_util_get_firmware_update_id(id));
	zassert_mem_equal(id, TEST_FW_UPDATE_ID, sizeof(id));
#endif
#endif
}

ZTEST(ctr_cloud_send_3_fw, test_update_id_wrong_length)
{
	static const uint8_t short_id[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	uint8_t id[16];

	/* Would leave half of the id uninitialized */
	zassert_ok(settings_save_one("cloud/firmware/update_id", short_id, sizeof(short_id)));
	zassert_equal(ctr_cloud_util_get_firmware_update_id(id), -ENOENT);

	zassert_ok(ctr_cloud_util_delete_firmware_update_id());
}
