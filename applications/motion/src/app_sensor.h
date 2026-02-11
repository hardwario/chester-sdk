/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#include "feature.h"
#include <zephyr/shell/shell.h>

#ifdef _cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

#if defined(CONFIG_CTR_S3)
int cmd_motion_view(const struct shell *shell, size_t argc, char **argv);
void app_sensor_print_last_sample(const struct shell *shell);
#endif

#ifdef _cplusplus
}
#endif

#endif /* defined (APP_SENSOR_H_) */
