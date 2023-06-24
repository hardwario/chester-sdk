/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

/* Zephyr includes */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(app_ble_svc, LOG_LEVEL_DBG);

static struct bt_uuid_128 m_svc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x450dad4c, 0xd8b4, 0x4dc4, 0xbdba, 0x498075184b3a));

static struct bt_uuid_128 m_interval_report_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xf6af9271, 0x67a9, 0x4bf8, 0xa618, 0x158af711b1b6));

static ssize_t write_interval_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	int ret;

	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	uint32_t value;

	if (len != sizeof(value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	value = sys_get_le32(buf);

	ret = app_config_set_interval_report(value);
	if (ret) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	return len;
}

static ssize_t read_interval_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				    void *buf, uint16_t len, uint16_t offset)
{
	uint32_t value;
	sys_put_le32(app_config_get_interval_report(), (uint8_t *)&value);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
}

/* clang-format off */

BT_GATT_SERVICE_DEFINE(
	app_svc,
	BT_GATT_PRIMARY_SERVICE(&m_svc_uuid),

	BT_GATT_CHARACTERISTIC(
		&m_interval_report_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
		read_interval_report, write_interval_report, NULL
	)
);

/* clang-format on */
