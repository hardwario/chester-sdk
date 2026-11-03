/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_power_internal.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_power.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_power_lrw, CONFIG_CTR_POWER_LOG_LEVEL);

int ctr_power_lrw_encode(struct ctr_buf *buf)
{
	int ret = 0;

	struct ctr_power_data *data = ctr_power_data_lock();

	if (isnan(data->system_voltage_rest)) {
		ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
	} else {
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(data->system_voltage_rest * 1000.f));
	}

	if (isnan(data->system_voltage_load)) {
		ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
	} else {
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(data->system_voltage_load * 1000.f));
	}

	if (isnan(data->system_current_load)) {
		ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
	} else {
		ret |= ctr_buf_append_u8(buf, (uint8_t)data->system_current_load);
	}

	/* The LoRaWAN payload carries no events; drop them as consumed. */
	if (!ret) {
		ctr_power_data_clear_events(data);
	}

	ctr_power_data_unlock();

	if (ret) {
		LOG_ERR("Call `ctr_buf_append` failed");
		return -EFAULT;
	}

	return 0;
}

int ctr_power_lrw_encode_external(struct ctr_buf *buf)
{
	int ret = 0;

	struct ctr_power_data *data = ctr_power_data_lock();

	if (isnan(data->line_voltage)) {
		ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
	} else {
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(data->line_voltage * 1000.f));
	}

	if (isnan(data->backup_voltage)) {
		ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
	} else {
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(data->backup_voltage * 1000.f));
	}

	ret |= ctr_buf_append_u8(buf, data->line == CTR_POWER_LINE_CONNECTED ? 1 : 0);

	/* The LoRaWAN payload carries no events; drop them as consumed. */
	if (!ret) {
		ctr_power_data_clear_events(data);
	}

	ctr_power_data_unlock();

	if (ret) {
		LOG_ERR("Call `ctr_buf_append` failed");
		return -EFAULT;
	}

	return 0;
}
