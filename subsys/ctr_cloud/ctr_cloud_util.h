/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_UTIL_H_
#define CHESTER_SUBSYS_CTR_CLOUD_UTIL_H_

#include "ctr_cloud_msg.h"

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_cloud_calculate_hash(uint8_t hash[8], const uint8_t *buf, size_t len);

int ctr_cloud_util_shell_cmd(const char *cmd, struct ctr_buf *buf);

int ctr_cloud_util_uuid_to_str(const ctr_cloud_uuid_t uuid, char *str, size_t len);

int ctr_cloud_util_str_to_uuid(const char *str, ctr_cloud_uuid_t uuid);

int ctr_cloud_util_save_firmware_update_id(const ctr_cloud_uuid_t uuid);

int ctr_cloud_util_get_firmware_update_id(ctr_cloud_uuid_t uuid);

int ctr_cloud_util_delete_firmware_update_id(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_UTIL_H_ */
