/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_config {
	int interval_sample;
	int interval_aggreg;
	int interval_report;
};

extern struct app_config g_app_config;

/* clang-format off */

#define CONFIG_PARAM_LIST_COMMON() \
	CONFIG_PARAM_INT(interval-sample, interval_sample, 1, 86400, "Get/Set sample interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(interval-aggreg, interval_aggreg, 1, 86400, "Get/Set aggregate interval in seconds (format: <1-86400>).") \
	CONFIG_PARAM_INT(interval-report, interval_report, 30, 86400, "Get/Set report interval in seconds (format: <30-86400>).")

/* clang-format on */

#define CONFIG_PARAM_LIST()                                                                        \
	CONFIG_PARAM_LIST_COMMON()

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help)                                      \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help)                                                 \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv);

CONFIG_PARAM_LIST();

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
