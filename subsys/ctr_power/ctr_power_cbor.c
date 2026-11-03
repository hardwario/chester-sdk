/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_power_internal.h"

/* CHESTER includes */
#include <chester/ctr_power.h>

#include <app_codec.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_power_cbor, CONFIG_CTR_POWER_LOG_LEVEL);

static void put_voltage_mv(zcbor_state_t *zs, float volts)
{
	if (isnan(volts)) {
		zcbor_nil_put(zs, NULL);
	} else {
		zcbor_int32_put(zs, (int32_t)(volts * 1000.f));
	}
}

static void encode_external(zcbor_state_t *zs, const struct ctr_power_data *data)
{
	zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL__SOURCE);
		zcbor_uint32_put(zs, data->source);

		if (data->source != CTR_POWER_SOURCE_NONE) {
			zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL__LINE);
			zcbor_uint32_put(zs, data->line);

			zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL__LINE_VOLTAGE);
			put_voltage_mv(zs, data->line_voltage);

			/* CHESTER-X4 has no backup battery, omit the key entirely. */
			if (data->source != CTR_POWER_SOURCE_CTR_X4_A &&
			    data->source != CTR_POWER_SOURCE_CTR_X4_B) {
				zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL__BACKUP_VOLTAGE);
				put_voltage_mv(zs, data->backup_voltage);
			}

			zcbor_uint32_put(zs, CODEC_KEY_E_POWER__EXTERNAL__EVENTS);
			{
				zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

				if (data->event_count) {
					int64_t timestamp_abs = data->events[0].timestamp;

					/* TSO absolute timestamp */
					zcbor_int64_put(zs, timestamp_abs);

					for (int i = 0; i < data->event_count; i++) {
						/* TSO offset timestamp */
						zcbor_int64_put(zs, data->events[i].timestamp -
									     timestamp_abs);
						zcbor_uint32_put(zs, data->events[i].line);
					}
				}

				zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
			}
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
}

static void encode_system(zcbor_state_t *zs, const struct ctr_power_data *data)
{
	zcbor_uint32_put(zs, CODEC_KEY_E_POWER__SYSTEM);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, CODEC_KEY_E_POWER__SYSTEM__VOLTAGE_REST);
		put_voltage_mv(zs, data->system_voltage_rest);

		zcbor_uint32_put(zs, CODEC_KEY_E_POWER__SYSTEM__VOLTAGE_LOAD);
		put_voltage_mv(zs, data->system_voltage_load);

		zcbor_uint32_put(zs, CODEC_KEY_E_POWER__SYSTEM__CURRENT_LOAD);
		if (isnan(data->system_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, (uint32_t)data->system_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
}

int ctr_power_cbor_encode(zcbor_state_t *zs)
{
	struct ctr_power_data *data = ctr_power_data_lock();

	zcbor_uint32_put(zs, CODEC_KEY_E_POWER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		encode_external(zs, data);
		encode_system(zs, data);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	bool ok = zcbor_check_error(zs);

	/* Consume the events under the same lock, so any event recorded meanwhile is kept. */
	if (ok) {
		ctr_power_data_clear_events(data);
	}

	ctr_power_data_unlock();

	if (!ok) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}
