/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_config.h"
#include "app_data.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(app_backup, LOG_LEVEL_DBG);

#if defined(FEATURE_HARDWARE_CHESTER_Z)

void handler_ctr_z(const struct device *dev, enum ctr_z_event backup_event, void *param)
{
	int ret;

	if (backup_event != CTR_Z_EVENT_DC_CONNECTED &&
	    backup_event != CTR_Z_EVENT_DC_DISCONNECTED) {
		return;
	}

	struct app_data_backup *backup = &g_app_data.backup;

	app_work_backup_update();

	app_data_lock();

	backup->line_present = backup_event == CTR_Z_EVENT_DC_CONNECTED;

	if (backup->event_count < APP_DATA_MAX_BACKUP_EVENTS) {
		struct app_data_backup_event *event = &backup->events[backup->event_count];

		ret = ctr_rtc_get_ts(&event->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return;
		}

		event->connected = backup->line_present;
		backup->event_count++;

		LOG_INF("Event count: %d", backup->event_count);
	} else {
		LOG_WRN("Measurement full");
		app_data_unlock();
		return;
	}

	LOG_INF("Backup: %d", (int)backup->line_present);

	if (g_app_config.backup_report_connected && backup_event == CTR_Z_EVENT_DC_CONNECTED) {
		app_work_send_with_rate_limit();
	}

	if (g_app_config.backup_report_disconnected &&
	    backup_event == CTR_Z_EVENT_DC_DISCONNECTED) {
		app_work_send_with_rate_limit();
	}

	app_data_unlock();
}

int app_backup_init(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_z_set_handler(dev, handler_ctr_z, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		return ret;
	}

	ret = ctr_z_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_z_get_serial_number(dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		return ret;
	}

	LOG_INF("Serial number: %08x", serial_number);

	uint16_t hw_revision;
	ret = ctr_z_get_hw_revision(dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW revision: %04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_z_get_hw_variant(dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW variant: %08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_z_get_fw_version(dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		return ret;
	}

	LOG_INF("FW version: %08x", fw_version);

	char vendor_name[32];
	ret = ctr_z_get_vendor_name(dev, vendor_name, sizeof(vendor_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vendor_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Vendor name: %s", vendor_name);

	char product_name[32];
	ret = ctr_z_get_product_name(dev, product_name, sizeof(product_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_product_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Product name: %s", product_name);

	return 0;
}

int app_backup_sample(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto error;
	}

	struct ctr_z_status status;
	ret = ctr_z_get_status(dev, &status);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
		goto error;
	}

	LOG_INF("DC input connected: %d", status.dc_input_connected);

	uint16_t vdc;
	ret = ctr_z_get_vdc_mv(dev, &vdc);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage DC input: %u mV", vdc);

	uint16_t vbat;
	ret = ctr_z_get_vbat_mv(dev, &vbat);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage backup battery: %u mV", vbat);

	app_data_lock();
	g_app_data.backup.line_present = status.dc_input_connected;
	g_app_data.backup.line_voltage = vdc / 1000.f;
	g_app_data.backup.battery_voltage = vbat / 1000.f;
	app_data_unlock();

	return 0;

error:
	app_data_lock();
	g_app_data.backup.line_present = false;
	g_app_data.backup.line_voltage = NAN;
	g_app_data.backup.battery_voltage = NAN;
	app_data_unlock();

	return ret;
}

int app_backup_clear(void)
{
	app_data_lock();
	g_app_data.backup.event_count = 0;
	app_data_unlock();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */
