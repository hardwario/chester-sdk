/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MODBUS_H_
#define APP_MODBUS_H_

#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)

#include <stdint.h>
#include <stddef.h>

/* Forward declarations */
struct shell;
struct app_data_sensecap;
struct app_data_cubic_pm;

#ifdef __cplusplus
extern "C" {
#endif

int app_modbus_enable(void);

int app_modbus_disable(void);
int app_modbus_sample(void);
int app_modbus_aggreg(void);
int app_modbus_clear(void);
int app_modbus_init(void);

int app_modbus_set_sensecap_addr(uint8_t current_addr, uint8_t new_addr);
int app_modbus_set_cubic_pm_addr(uint8_t current_addr, uint8_t new_addr);

/* Shell command handlers (called from app_shell.c) */
int cmd_modbus_set_sensecap(const struct shell *shell, size_t argc, char **argv);
int cmd_modbus_set_cubic(const struct shell *shell, size_t argc, char **argv);
int cmd_modbus_read(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_M) */

#endif /* APP_MODBUS_H_ */
