/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>

struct data g_app_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.acceleration_x = NAN,
	.acceleration_y = NAN,
	.acceleration_z = NAN,
	.orientation = INT_MAX,
};

K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
