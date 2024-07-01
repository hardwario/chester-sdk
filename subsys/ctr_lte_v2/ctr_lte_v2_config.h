/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_CONFIG_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_v2_config_antenna {
	CTR_LTE_V2_CONFIG_ANTENNA_INT = 0,
	CTR_LTE_V2_CONFIG_ANTENNA_EXT = 1,
};

enum ctr_lte_v2_config_auth {
	CTR_LTE_V2_CONFIG_AUTH_NONE = 0,
	CTR_LTE_V2_CONFIG_AUTH_PAP = 1,
	CTR_LTE_V2_CONFIG_AUTH_CHAP = 2,
};

struct ctr_lte_v2_config {
	bool test;
	enum ctr_lte_v2_config_antenna antenna;
	bool nb_iot_mode;
	bool lte_m_mode;
	bool autoconn;
	char plmnid[6 + 1];
	bool clksync;
	char apn[63 + 1];
	enum ctr_lte_v2_config_auth auth;
	char username[32 + 1];
	char password[32 + 1];
	char addr[15 + 1];
	int port;
	bool modemtrace;
};

extern struct ctr_lte_v2_config g_ctr_lte_v2_config;

int ctr_lte_v2_config_cmd_show(const struct shell *shell, size_t argc, char **argv);
int ctr_lte_v2_config_cmd(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_CONFIG_H_ */
