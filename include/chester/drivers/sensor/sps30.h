/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CHESTER_DRIVERS_SENSOR_SPS30_H_
#define CHESTER_INCLUDE_CHESTER_DRIVERS_SENSOR_SPS30_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

enum sensor_channel_sps30 {
	SENSOR_CHAN_SPS30_PM_4_0 = SENSOR_CHAN_PRIV_START,
	SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_0_5,
	SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_1_0,
	SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_2_5,
	SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_4_0,
	SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_10_0,
	SENSOR_CHAN_SPS30_TYP_PART_SIZE,
};

int sps30_power_on_init(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CHESTER_DRIVERS_SENSOR_SPS30_H_ */
