/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Mock MCUboot image state for the firmware confirmation of ctr_cloud.c (compiled with
 * CONFIG_MCUBOOT_IMG_MANAGER) and the firmware update scenario selected by TEST_FW.
 */

#include "common.h"
#include "ctr_cloud_msg.h"
#include "ctr_cloud_util.h"

/* Zephyr includes */
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>

const uint8_t TEST_FW_UPDATE_ID[16] = {0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87,
				       0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f};

static bool m_confirmed = true;
static int m_confirm_calls;

bool boot_is_img_confirmed(void)
{
	return m_confirmed;
}

int boot_write_img_confirmed(void)
{
	m_confirm_calls++;

#if TEST_FW == 4
	/* The flash write fails */
	return -EIO;
#else
	m_confirmed = true;

	return 0;
#endif
}

int mock_boot_confirm_calls(void)
{
	return m_confirm_calls;
}

void mock_fw_setup(void)
{
	zassert_ok(settings_subsys_init());

	/* The flash simulator keeps its content from a previous run */
	zassert_ok(ctr_cloud_util_delete_firmware_update_id());

#if TEST_FW == 3
	/* MCUboot reverted the update: the old image runs, confirmed */
	m_confirmed = true;
#else
	/* Just swapped in by MCUboot and not confirmed yet */
	m_confirmed = false;
#endif

#if TEST_FW >= 1
	zassert_ok(ctr_cloud_util_save_firmware_update_id(TEST_FW_UPDATE_ID));
#endif

#if TEST_FW == 2
	mock_transfer_fail_first_of_type(UL_UPLOAD_FIRMWARE, -EIO, 1);
#endif
}
