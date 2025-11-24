/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_lrw.h"
#include "app_config.h"
#include "app_data.h"

#include <chester/ctr_encode_lrw.h>
#include <chester/ctr_ble_tag.h>

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_lrw, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)
int app_lrw_encode(struct ctr_buf *buf)
{
	int ret = 0;

	ctr_buf_reset(buf);

	app_data_lock();

	uint16_t header = 0;

	header |= IS_ENABLED(CONFIG_CTR_BATT) ? BIT(0) : 0;
	header |= IS_ENABLED(CONFIG_CTR_ACCEL) ? BIT(1) : 0;
	header |= IS_ENABLED(CONFIG_CTR_THERM) ? BIT(2) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_METEO_A) ? BIT(3) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_METEO_B) ? BIT(3) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_S2) ? BIT(4) : 0;
	header |= IS_ENABLED(FEATURE_SUBSYSTEM_DS18B20) ? BIT(5) : 0;
	header |= IS_ENABLED(FEATURE_SUBSYSTEM_BLE_TAG) ? BIT(6) : 0;
	// Bit 7 is reserved for extended header
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) ? BIT(8) : 0;

	// If we used extended header
	if (header >= BIT(8)) {
		header |= BIT(7); // Set extended header flag
	}

	// Write standard header
	ret |= ctr_buf_append_u8(buf, header);

	// Write extended header if needed
	if (header & BIT(7)) {
		ret |= ctr_buf_append_u8(buf, 0xffff & (header >> 8));
	}

#if defined(CONFIG_CTR_BATT)
	CTR_ENCODE_LRW_BATTERY(buf);
#endif

#if defined(CONFIG_CTR_ACCEL)
	CTR_ENCODE_LRW_ACCEL(buf);
#endif

#if defined(CONFIG_CTR_THERM)
	CTR_ENCODE_LRW_THERM(buf);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	CTR_ENCODE_LRW_METEO(buf);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	CTR_ENCODE_LRW_S2(buf);
#endif

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	CTR_ENCODE_LRW_W1_THERM(buf);
#endif

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	CTR_ENCODE_LRW_BLE_TAG(buf);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	CTR_ENCODE_LRW_BAROMETER_TAG(buf);
#endif

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */
