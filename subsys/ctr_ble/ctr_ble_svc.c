/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_config.h>
#include <chester/ctr_info.h>

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

LOG_MODULE_REGISTER(ctr_ble_svc, CONFIG_CTR_BLE_LOG_LEVEL);

enum {
	COMMAND_REBOOT = 0,
	COMMAND_CONFIG_SAVE = 1,
	COMMAND_CONFIG_RESET = 2,
};

static struct bt_uuid_128 m_generic_svc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xe5faf1ac, 0x5d45, 0x44bb, 0x97af, 0xc91b7b0990ad));

static struct bt_uuid_128 m_command_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0c0ed4e6, 0xf4b0, 0x4d07, 0x9df7, 0xb77d46719d2f));

static struct bt_uuid_128 m_uptime_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xb2fbcfc3, 0xb383, 0x4b27, 0xb5b5, 0x7ee56aff25ef));

static ssize_t write_command(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
			     uint16_t len, uint16_t offset, uint8_t flags)
{
	uint8_t type;

	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	type = *(const uint8_t *)buf;

	if (type == COMMAND_REBOOT && len == 1) {
		LOG_INF("Command `REBOOT`");
		sys_reboot(SYS_REBOOT_COLD);
		return len;
	}

	if (type == COMMAND_CONFIG_SAVE && len == 1) {
		LOG_INF("Command `CONFIG SAVE`");
		ctr_config_save();
		return len;
	}

	if (type == COMMAND_CONFIG_RESET && len == 1) {
		LOG_INF("Command `CONFIG RESET`");
		ctr_config_reset();
		return len;
	}

	return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
}

static ssize_t read_uptime(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	uint64_t timeout = sys_cpu_to_le64(k_uptime_get() / 1000);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &timeout, sizeof(timeout));
}

/* clang-format off */

BT_GATT_SERVICE_DEFINE(
	generic_svc,
	BT_GATT_PRIMARY_SERVICE(&m_generic_svc_uuid),

	BT_GATT_CHARACTERISTIC(
		&m_command_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE_AUTHEN,
		NULL, write_command, NULL
	),

	BT_GATT_CHARACTERISTIC(
		&m_uptime_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ_AUTHEN,
		read_uptime, NULL, NULL
	),

	BT_GATT_CCC(NULL, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),

);

/* clang-format on */

static void uptime_notify_work_handler(struct k_work *work)
{
	uint64_t timeout = sys_cpu_to_le64(k_uptime_get() / 1000);

	bt_gatt_notify_uuid(NULL, &m_uptime_chrc_uuid.uuid, generic_svc.attrs, &timeout,
			    sizeof(timeout));
}

static K_WORK_DEFINE(m_uptime_notify_work, uptime_notify_work_handler);

static void uptime_notify_timer(struct k_timer *timer_id)
{
	k_work_submit(&m_uptime_notify_work);
}

static K_TIMER_DEFINE(m_uptime_notify_timer, uptime_notify_timer, NULL);

static int init(void)
{
	k_timer_start(&m_uptime_notify_timer, K_NO_WAIT, K_SECONDS(5));

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
