/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_BLE_CLIENT_H_
#define CHESTER_SUBSYS_CTR_BLE_CLIENT_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

/* Zephyr includes */
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

void ctr_ble_client_cb_connected(struct bt_conn *conn, uint8_t err);
void ctr_ble_client_cb_disconnected(struct bt_conn *conn, uint8_t reason);
bool ctr_ble_client_cb_le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param);
void ctr_ble_client_cb_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
					uint16_t timeout);
void ctr_ble_client_cb_auth_passkey_entry(struct bt_conn *conn);

int ctr_ble_client_neighbor_reboot(char *addr, unsigned long passkey, const struct shell *shell);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_BLE_CLIENT_H_ */
