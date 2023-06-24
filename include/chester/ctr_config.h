/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_CONFIG_H_
#define CHESTER_INCLUDE_CTR_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard include */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ctr_config_show_cb)(const struct shell *shell, size_t argc, char **argv);

int ctr_config_save(void);
int ctr_config_reset(void);
void ctr_config_append_show(const char *name, ctr_config_show_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CONFIG_H_ */
