/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_CONFIG_H_
#define CHESTER_SUBSYS_CTR_CLOUD_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_cloud_config {
#if defined(CONFIG_CTR_CLOUD_SPOOL)
	int spool_size;
#endif
};

extern struct ctr_cloud_config g_ctr_cloud_config;

int ctr_cloud_config_cmd_show(const struct shell *shell, size_t argc, char **argv);
int ctr_cloud_config_cmd(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_CONFIG_H_ */
