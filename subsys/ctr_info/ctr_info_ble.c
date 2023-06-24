/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_info.h>

/* Zephyr includes */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ctr_info_ble, CONFIG_CTR_INFO_LOG_LEVEL);

static struct bt_uuid_128 m_info_svc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x8f4afdba, 0xae12, 0x465b, 0xa871, 0x13bd7182017c));

static struct bt_uuid_128 m_vendor_name_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6e7e8548, 0x7481, 0x4822, 0xa238, 0x7607ad2f667d));

static struct bt_uuid_128 m_product_name_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xb3249503, 0x1cc7, 0x4200, 0xa9c1, 0x3d8ab2c3235c));

static struct bt_uuid_128 m_hw_variant_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xadc8c4d2, 0xef7c, 0x425a, 0xaf5b, 0x1ce9c94dc51c));

static struct bt_uuid_128 m_hw_revision_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x12e740c2, 0x5bf4, 0x48f6, 0xa781, 0x8b064b21622c));

static struct bt_uuid_128 m_fw_name_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0e5f5dfe, 0xafef, 0x4725, 0xb19f, 0x92e47801c721));

static struct bt_uuid_128 m_fw_version_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x56917801, 0xe1a0, 0x476a, 0xab86, 0x07e61076d6d9));

static struct bt_uuid_128 m_serial_number_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0cd70392, 0x53b9, 0x4518, 0xb155, 0xf5f7474ec28e));

static struct bt_uuid_128 m_claim_token_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xa24a59ae, 0x98d5, 0x4c20, 0xaa0e, 0x0dc0fde88b96));

static struct bt_uuid_128 m_ble_devaddr_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xe5c97616, 0x10fe, 0x4f04, 0x988a, 0x8f904a81f974));

static struct bt_uuid_128 m_ble_passkey_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x18c18da9, 0xddbc, 0x49be, 0xa241, 0x1144aab2b5fa));

static ssize_t pass_str(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			uint16_t len, uint16_t offset)
{
	int ret;

	char *s;
	ret = ((int (*)(char **))attr->user_data)(&s);
	if (ret) {
		LOG_ERR("Call failed: %d", ret);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, s, strlen(s));
}

/* clang-format off */

BT_GATT_SERVICE_DEFINE(
	info_svc,
	BT_GATT_PRIMARY_SERVICE(&m_info_svc_uuid),

	BT_GATT_CHARACTERISTIC(
		&m_vendor_name_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_vendor_name
	),

	BT_GATT_CHARACTERISTIC(
		&m_product_name_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_product_name
	),

	BT_GATT_CHARACTERISTIC(
		&m_hw_variant_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_hw_variant
	),

	BT_GATT_CHARACTERISTIC(
		&m_hw_revision_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_hw_revision
	),

	BT_GATT_CHARACTERISTIC(
		&m_fw_name_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_fw_name
	),

	BT_GATT_CHARACTERISTIC(
		&m_fw_version_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_fw_version
	),

	BT_GATT_CHARACTERISTIC(
		&m_serial_number_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_serial_number
	),

	BT_GATT_CHARACTERISTIC(
		&m_claim_token_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_claim_token
	),

	BT_GATT_CHARACTERISTIC(
		&m_ble_devaddr_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_ble_devaddr
	),

	BT_GATT_CHARACTERISTIC(
		&m_ble_passkey_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ_AUTHEN,
		pass_str, NULL, ctr_info_get_ble_passkey
	),
);

/* clang-format on */
