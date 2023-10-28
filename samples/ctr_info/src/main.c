/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_info.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <inttypes.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	char *vendor_name;
	ret = ctr_info_get_vendor_name(&vendor_name);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_vendor_name` failed: %d", ret);
	} else {
		LOG_INF("Vendor name: %s", vendor_name);
	}

	char *product_name;
	ret = ctr_info_get_product_name(&product_name);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_product_name` failed: %d", ret);
	} else {
		LOG_INF("Product name: %s", product_name);
	}

	char *hw_variant;
	ret = ctr_info_get_hw_variant(&hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_hw_variant` failed: %d", ret);
	} else {
		LOG_INF("Hardware variant: %s", hw_variant);
	}

	char *hw_revision;
	ret = ctr_info_get_hw_revision(&hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_hw_revision` failed: %d", ret);
	} else {
		LOG_INF("Hardware revision: %s", hw_revision);
	}

	char *fw_name;
	ret = ctr_info_get_fw_name(&fw_name);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_fw_name` failed: %d", ret);
	} else {
		LOG_INF("Firmware name: %s", fw_name);
	}

	char *fw_version;
	ret = ctr_info_get_fw_version(&fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_fw_version` failed: %d", ret);
	} else {
		LOG_INF("Firmware version: %s", fw_version);
	}

	char *serial_number;
	ret = ctr_info_get_serial_number(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number` failed: %d", ret);
	} else {
		LOG_INF("Serial number: %s", serial_number);
	}

	uint32_t serial_number_uint32;
	ret = ctr_info_get_serial_number_uint32(&serial_number_uint32);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
	} else {
		LOG_INF("Serial number (uint32): %" PRIu32, serial_number_uint32);
	}

	char *claim_token;
	ret = ctr_info_get_claim_token(&claim_token);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_claim_token` failed: %d", ret);
	} else {
		LOG_INF("Claim token: %s", claim_token);
	}

	char *ble_devaddr;
	ret = ctr_info_get_ble_devaddr(&ble_devaddr);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_ble_devaddr` failed: %d", ret);
	} else {
		LOG_INF("BLE device address: %s", ble_devaddr);
	}

	uint64_t ble_devaddr_uint64;
	ret = ctr_info_get_ble_devaddr_uint64(&ble_devaddr_uint64);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_ble_devaddr_uint64` failed: %d", ret);
	} else {
		LOG_INF("BLE device address (uint64): %" PRIu64, ble_devaddr_uint64);
	}

	char *ble_passkey;
	ret = ctr_info_get_ble_passkey(&ble_passkey);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_ble_passkey` failed: %d", ret);
	} else {
		LOG_INF("BLE passkey: %s", ble_passkey);
	}

	return 0;
}
