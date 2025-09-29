/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_ble_client.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ctr_ble_client, LOG_LEVEL_INF);

K_MUTEX_DEFINE(m_mutex);
uint8_t m_selected_id = BT_ID_DEFAULT;
static const struct shell *m_ctx_shell = NULL;
static struct bt_conn *m_default_conn = NULL;
K_SEM_DEFINE(m_sem, 0, 1);
int m_handle_command = -1;
int m_handle_uptime = -1;
unsigned int m_passkey = 0;

#define INF(...)                                                                                   \
	if (m_ctx_shell) {                                                                         \
		shell_print(m_ctx_shell, __VA_ARGS__);                                             \
	}                                                                                          \
	LOG_INF(__VA_ARGS__);

#define ERR(...)                                                                                   \
	if (m_ctx_shell) {                                                                         \
		shell_error(m_ctx_shell, __VA_ARGS__);                                             \
	}                                                                                          \
	LOG_ERR(__VA_ARGS__);

static void conn_addr_str(struct bt_conn *conn, char *addr, size_t len)
{
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) < 0) {
		addr[0] = '\0';
		return;
	}

	switch (info.type) {
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_BR:
		bt_addr_to_str(info.br.dst, addr, len);
		break;
#endif
	case BT_CONN_TYPE_LE:
		bt_addr_le_to_str(info.le.dst, addr, len);
		break;
	default:
		break;
	}
}

void ctr_ble_client_cb_connected(struct bt_conn *conn, uint8_t err)
{
	if (conn == NULL) {
		LOG_ERR("Connection object is NULL");
		return;
	}

	char addr[BT_ADDR_LE_STR_LEN];
	conn_addr_str(conn, addr, sizeof(addr));

	if (err) {
		ERR("Failed to connect to %s (0x%02x)", addr, err);
		return;
	}

	INF("Connected: %s", addr);

	if (m_default_conn != NULL) {
		bt_conn_unref(m_default_conn);
	}
	m_default_conn = bt_conn_ref(conn);
}

void ctr_ble_client_cb_disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (conn == NULL) {
		LOG_ERR("Connection object is NULL");
		return;
	}

	char addr[BT_ADDR_LE_STR_LEN];
	conn_addr_str(conn, addr, sizeof(addr));

	INF("Disconnected: %s (reason 0x%02x)", addr, reason);

	if (m_default_conn == conn) {
		m_default_conn = NULL;
	}

	bt_conn_unref(conn);
}

bool ctr_ble_client_cb_le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	INF("LE conn param req: int (0x%04x, 0x%04x) lat %d to %d", param->interval_min,
	    param->interval_max, param->latency, param->timeout);
	return true;
}

void ctr_ble_client_cb_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
					uint16_t timeout)
{
	INF("LE conn param updated: int 0x%04x lat %d to %d", interval, latency, timeout);

	k_sem_give(&m_sem);
}

void ctr_ble_client_cb_auth_passkey_entry(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	conn_addr_str(conn, addr, sizeof(addr));
	INF("Requested passkey entry from %s", addr);

	if (conn != m_default_conn) {
		ERR("Invalid connection");
		return;
	}

	int ret = bt_conn_auth_passkey_entry(conn, m_passkey);
	if (ret < 0) {
		ERR("Failed to enter passkey: %d", ret);
	}

	INF("Passkey entered");
}

static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	if (!attr) {
		LOG_INF("Discover complete");
		return BT_GATT_ITER_STOP;
	}

	if (params) {
		if (params->type == BT_GATT_DISCOVER_ATTRIBUTE) {

			static struct bt_uuid_128 m_command_chrc_uuid =
				BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0c0ed4e6, 0xf4b0, 0x4d07,
								    0x9df7, 0xb77d46719d2f));

			static struct bt_uuid_128 m_uptime_chrc_uuid =
				BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xb2fbcfc3, 0xb383, 0x4b27,
								    0xb5b5, 0x7ee56aff25ef));

			if (bt_uuid_cmp(attr->uuid, &m_command_chrc_uuid.uuid) == 0) {
				m_handle_command = attr->handle;
			} else if (bt_uuid_cmp(attr->uuid, &m_uptime_chrc_uuid.uuid) == 0) {
				m_handle_uptime = attr->handle;
			}

			if (m_handle_command >= 0 && m_handle_uptime >= 0) {
				k_sem_give(&m_sem);
				return BT_GATT_ITER_STOP;
			}
			/* char str[BT_UUID_STR_LEN]; */
			/* bt_uuid_to_str(attr->uuid, str, sizeof(str)); */
			/* LOG_DBG("Descriptor %s found: handle %x", str, attr->handle); */
		}
	}

	return BT_GATT_ITER_CONTINUE;
}

static void write_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	LOG_INF("Write complete: err 0x%02x", err);
	k_sem_give(&m_sem);
}

static int reboot(char *addr, unsigned long passkey)
{
	int ret;
	struct bt_conn *conn;
	bt_addr_le_t ble_addr;

	ret = bt_addr_le_from_str(addr, "random", &ble_addr);
	if (ret < 0) {
		ERR("Failed to parse address failed: %d", ret);
		return ret;
	}

	m_passkey = passkey;

	bt_le_scan_stop();

	ret = bt_unpair(m_selected_id, &ble_addr);
	if (ret < 0) {
		ERR("Failed to clear pairings failed: %d", ret);
	} else {
		INF("Pairings successfully cleared");
	}

	k_sem_take(&m_sem, K_NO_WAIT); /* reset */
	m_default_conn = NULL;

	INF("Connecting...");

	for (int i = 0; i < 3; i++) {
		ret = bt_conn_le_create(&ble_addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT,
					&conn);
		if (ret < 0) {
			ERR("Failed to create connection: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
		}

		k_sem_take(&m_sem, K_MSEC(10000));

		if (m_default_conn != NULL) {
			break;
		}

		if (conn != NULL) {
			bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			bt_conn_unref(conn);
			conn = NULL;
		}

		k_sleep(K_MSEC(3000));
	}

	if (m_default_conn == NULL) {
		ERR("Failed to connect");
		return -1;
	}

	m_selected_id = bt_conn_index(m_default_conn);

	INF("Connected");

	static struct bt_gatt_discover_params discover_params;

	discover_params.func = discover_func;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
#if defined(CONFIG_BT_EATT)
	discover_params.chan_opt = BT_ATT_CHAN_OPT_NONE;
#endif /* CONFIG_BT_EATT */
	discover_params.type = BT_GATT_DISCOVER_ATTRIBUTE;

	ret = bt_gatt_discover(m_default_conn, &discover_params);
	if (ret < 0) {
		ERR("Discover failed failed: %d", ret);
	} else {
		INF("Discover pending");
	}

	k_sem_take(&m_sem, K_MSEC(10000));

	if (m_handle_command < 0 || m_handle_uptime < 0) {
		ERR("Failed to find handles");
		return -1;
	}

	INF("Command handle: %x", m_handle_command);
	INF("Uptime handle: %x", m_handle_uptime);

	uint8_t write_buf[] = {0x02};
	static struct bt_gatt_write_params write_params;
	write_params.length = sizeof(write_buf);
	write_params.data = write_buf;
	write_params.handle = m_handle_command;
	write_params.offset = 0;
	write_params.func = write_func;

	ret = bt_gatt_write(m_default_conn, &write_params);
	if (ret < 0) {
		ERR("Failed to write command failed: %d", ret);
		return ret;
	}

	k_sem_take(&m_sem, K_MSEC(10000));

	INF("Command reboot sent");

	ret = bt_unpair(m_selected_id, &ble_addr);
	if (ret < 0) {
		ERR("Failed to clear pairings failed: %d", ret);
	} else {
		INF("Pairings successfully cleared");
	}

	m_selected_id = BT_ID_DEFAULT;

	return 0;
}

int ctr_ble_client_neighbor_reboot(char *addr, unsigned long passkey, const struct shell *shell)
{
	k_mutex_lock(&m_mutex, K_FOREVER);
	m_ctx_shell = shell;
	int ret = reboot(addr, passkey);
	m_ctx_shell = NULL;
	k_mutex_unlock(&m_mutex);
	return ret;
}
