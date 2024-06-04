/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_BLE_TAG_H_
#define CHESTER_INCLUDE_CTR_BLE_TAG_H_

/* Zephyr includes */
#include <zephyr/bluetooth/bluetooth.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_BLE_TAG_COUNT 8

#define CTR_BLE_TAG_ENROLL_TIMEOUT_SEC 10

#define CTR_BLE_TAG_ENROLL_RSSI_THRESHOLD -55

#define CTR_BLE_TAG_DATA_TIMEOUT_INTERVALS 3

#define CTR_BLE_TAG_SCAN_MAX_TIME 0x4000

/**
 * @addtogroup ctr_ble_tag ctr_ble_tag
 * @{
 */

int ctr_ble_tag_read(int idx, uint8_t addr[BT_ADDR_SIZE], int *rssi, float *voltage,
		     float *temperature, float *humidity, bool *valid);

int ctr_ble_tag_is_addr_empty(const uint8_t addr[BT_ADDR_SIZE]);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_BLE_TAG_H_ */
