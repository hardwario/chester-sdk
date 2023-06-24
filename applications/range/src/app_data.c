/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stdbool.h>

struct app_data g_app_data = {
	.system_voltage_rest = NAN,
	.system_voltage_load = NAN,
	.system_current_load = NAN,
	.accel_acceleration_x = NAN,
	.accel_acceleration_y = NAN,
	.accel_acceleration_z = NAN,
	.accel_orientation = INT_MAX,
	.therm_temperature = NAN,
};

static K_MUTEX_DEFINE(m_lock);

void app_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
}

void app_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}

K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
