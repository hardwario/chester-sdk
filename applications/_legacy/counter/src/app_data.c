/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "msg_key.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>

struct app_data g_app_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.accel_x = NAN,
	.accel_y = NAN,
	.accel_z = NAN,
	.accel_orientation = INT_MAX,
};

K_MUTEX_DEFINE(g_app_data_lock);

K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
