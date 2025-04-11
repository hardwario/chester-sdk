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

#if defined(CONFIG_CTR_BLE_TAG_32_SLOTS)
#define CTR_BLE_TAG_COUNT 32
#else
#define CTR_BLE_TAG_COUNT 8
#endif

#define CTR_BLE_TAG_ENROLL_TIMEOUT_SEC 12

#define CTR_BLE_TAG_ENROLL_RSSI_THRESHOLD -40

#define CTR_BLE_TAG_DATA_TIMEOUT_INTERVALS 3

#define CTR_BLE_TAG_SCAN_MAX_TIME 0x4000

#define CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE          BIT(0)
#define CTR_BLE_TAG_SENSOR_MASK_HUMIDITY             BIT(1)
#define CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED      BIT(2)
#define CTR_BLE_TAG_SENSOR_MASK_MOVING               BIT(3)
#define CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT BIT(4)
#define CTR_BLE_TAG_SENSOR_MASK_ROLL                 BIT(5)
#define CTR_BLE_TAG_SENSOR_MASK_PITCH                BIT(6)
#define CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY          BIT(7)
#define CTR_BLE_TAG_SENSOR_MASK_VOLTAGE              BIT(8)

/**
 * @addtogroup ctr_ble_tag ctr_ble_tag
 * @{
 */

int ctr_ble_tag_enable(bool enabled);
int ctr_ble_tag_add(char *addr);
int ctr_ble_tag_remove_addr(char *addr);
int ctr_ble_tag_remove_slot(size_t slot);
void ctr_ble_tag_remove_all(void);

int ctr_ble_tag_set_scan_interval(int scan_interval);
int ctr_ble_tag_get_scan_interval(void);
int ctr_ble_tag_set_scan_duration(int scan_duration);
int ctr_ble_tag_get_scan_duration(void);

int ctr_ble_tag_read_cached(size_t slot, uint8_t addr[BT_ADDR_SIZE], int8_t *rssi, float *voltage,
			    float *temperature, float *humidity, bool *magnet_detected,
			    bool *moving, int *movement_event_count, float *roll, float *pitch,
			    bool *low_battery, int16_t *sensor_mask, bool *valid);

int ctr_ble_tag_is_addr_empty(const uint8_t addr[BT_ADDR_SIZE]);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_BLE_TAG_H_ */
