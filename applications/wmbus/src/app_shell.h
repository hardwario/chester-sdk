/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SHELL_H_
#define APP_SHELL_H_

#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

struct shell *app_shell_get(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SHELL_H_ */
