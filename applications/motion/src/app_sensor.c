/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_sensor.h"
#include "app_work.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/mb7066.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG);

/*static int compare(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;

    return (fa > fb) - (fa < fb);
}
*/
int app_sensor_sample(void)
{
    int ret = 0;
    float therm_temperature;

    ret = ctr_therm_read(&therm_temperature);
    if (ret) {
       LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
       g_app_data.therm_temperature = NAN;
    }

    float accel_acceleration_x;
    float accel_acceleration_y;
    float accel_acceleration_z;
    int accel_orientation;

    ret = ctr_accel_read(&accel_acceleration_x, &accel_acceleration_y, &accel_acceleration_z,
               &accel_orientation);
    if (ret) {
       LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
       accel_acceleration_x = NAN;
       accel_acceleration_y = NAN;
       accel_acceleration_z = NAN;
       accel_orientation = INT_MAX;
    }

    app_data_lock();
    g_app_data.therm_temperature = therm_temperature;
    g_app_data.accel_acceleration_x = accel_acceleration_x;
    g_app_data.accel_acceleration_y = accel_acceleration_y;
    g_app_data.accel_acceleration_z = accel_acceleration_z;
    g_app_data.accel_orientation = accel_orientation;

#if defined(CONFIG_CTR_S3)
    uint64_t timestamp;
    ret = ctr_rtc_get_ts(&timestamp);
    if (ret) {
        LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
        timestamp = 0;
    }

    if (g_app_data.motion_sample_count < APP_DATA_MAX_MEASUREMENTS) {
	g_app_data.motion_samples[g_app_data.motion_sample_count].timestamp = timestamp;
        g_app_data.motion_samples[g_app_data.motion_sample_count].value.detect_left = g_app_data.motion_count_l;
        g_app_data.motion_samples[g_app_data.motion_sample_count].value.detect_right = g_app_data.motion_count_r;
        g_app_data.motion_samples[g_app_data.motion_sample_count].value.motion_left = g_app_data.motion_count_left;
        g_app_data.motion_samples[g_app_data.motion_sample_count].value.motion_right = g_app_data.motion_count_right;
        g_app_data.motion_sample_count++;
        LOG_DBG("Sample added. Counts: %d, %d, %d, %d", g_app_data.motion_count_l, g_app_data.motion_count_r, g_app_data.motion_count_left, g_app_data.motion_count_right);
    }


    g_app_data.motion_count_l = 0;
    g_app_data.motion_count_r = 0;
    g_app_data.motion_count_left = 0;
    g_app_data.motion_count_right = 0;
#endif /* defined(CONFIG_CTR_S3) */

    app_data_unlock();

    return ret;
}

#if defined(CONFIG_CTR_S3)
static void print_motion_sample(const struct shell *shell, int idx)
{
	time_t ts = (time_t)g_app_data.motion_samples[idx].timestamp;
	struct tm *tm_info = gmtime(&ts);
	char time_buf[20];
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

	shell_print(shell, "[%d] L=%d, R=%d, L->R=%d, R->L=%d, %s",
		    idx,
		    g_app_data.motion_samples[idx].value.detect_left,
		    g_app_data.motion_samples[idx].value.detect_right,
		    g_app_data.motion_samples[idx].value.motion_right,
		    g_app_data.motion_samples[idx].value.motion_left,
		    time_buf);
}

int cmd_motion_view(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_data_lock();

	shell_print(shell, "Motion Samples (%d total):", g_app_data.motion_sample_count);
	shell_print(shell, "--TOTAL- L=%d, R=%d, L->R=%d, R->L=%d",
		    g_app_data.total_detect_left,
		    g_app_data.total_detect_right,
		    g_app_data.total_motion_right,
		    g_app_data.total_motion_left);
	shell_print(shell, "");

	for (int i = 0; i < g_app_data.motion_sample_count; i++) {
		print_motion_sample(shell, i);
	}

	app_data_unlock();

	return 0;
}

void app_sensor_print_last_sample(const struct shell *shell)
{
	app_data_lock();
	int idx = g_app_data.motion_sample_count - 1;
	if (idx >= 0) {
		print_motion_sample(shell, idx);
	}
	app_data_unlock();
}
#endif /* defined(CONFIG_CTR_S3) */
