/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_INFO_H_
#define CHESTER_INCLUDE_CTR_INFO_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_info_get_vendor_name(char **vendor_name);
int ctr_info_get_product_name(char **product_name);
int ctr_info_get_hw_variant(char **hw_variant);
int ctr_info_get_hw_revision(char **hw_revision);
int ctr_info_get_fw_name(char **fw_name);
int ctr_info_get_fw_version(char **fw_version);
int ctr_info_get_serial_number(char **serial_number);
int ctr_info_get_serial_number_uint32(uint32_t *serial_number);
int ctr_info_get_claim_token(char **claim_token);
int ctr_info_get_ble_devaddr(char **ble_devaddr);
int ctr_info_get_ble_devaddr_uint64(uint64_t *ble_devaddr);
int ctr_info_get_ble_passkey(char **ble_passkey);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_INFO_H_ */
