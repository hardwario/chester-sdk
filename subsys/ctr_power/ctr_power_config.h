/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CTR_POWER_CONFIG_H_
#define CTR_POWER_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_power_config_source {
	CTR_POWER_CONFIG_SOURCE_AUTO = 0,
	CTR_POWER_CONFIG_SOURCE_NONE = 1,
	CTR_POWER_CONFIG_SOURCE_CTR_Z = 2,
	CTR_POWER_CONFIG_SOURCE_CTR_X4_A = 3,
	CTR_POWER_CONFIG_SOURCE_CTR_X4_B = 4,
	CTR_POWER_CONFIG_SOURCE_CTR_X10 = 5,
};

struct ctr_power_config {
	enum ctr_power_config_source source;
	int interval_sample;
	bool report_connected;
	bool report_disconnected;
};

extern struct ctr_power_config g_ctr_power_config;

int ctr_power_config_cmd_show(const struct shell *shell, size_t argc, char **argv);
int ctr_power_config_cmd(const struct shell *shell, size_t argc, char **argv);
int ctr_power_config_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CTR_POWER_CONFIG_H_ */
