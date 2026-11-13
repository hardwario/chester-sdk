/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_microsens_180hs_proto.h
 * @brief Pure MicroSENS 180-HS frame/response parser (no Zephyr / app dependencies)
 *
 * Splits the STX/ETX frame extraction and the measurement-string parsing out of the
 * driver so they can be unit-tested on the host. The transport (send/receive over
 * app_serial) stays in the driver.
 *
 * Response frame: STX <ASCII payload> ETX.
 * Measurement payload: "<sensor_id> <timestamp> <co2_raw> <temp_raw> <pressure>".
 */

#ifndef DRV_MICROSENS_180HS_PROTO_H_
#define DRV_MICROSENS_180HS_PROTO_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_MICROSENS_180HS_STX 0x02
#define DRV_MICROSENS_180HS_ETX 0x03

/**
 * @brief Extract the payload between STX and ETX from a received buffer
 *
 * Finds the first STX, then the first ETX after it, and copies the bytes in between to
 * @p out, NUL-terminating. @p out must hold at least @p max_len + 1 bytes.
 *
 * @param buf     Received bytes
 * @param buf_len Number of bytes in @p buf
 * @param out     Destination for the extracted payload (NUL-terminated on success)
 * @param max_len Maximum payload length (excluding the terminator)
 * @param out_len Output: extracted payload length
 * @retval 0 on success
 * @retval -EINVAL if a pointer argument is NULL
 * @retval -EIO    if STX, or ETX after STX, is missing
 * @retval -ENOMEM if the payload is longer than @p max_len
 */
int drv_microsens_180hs_extract_frame(const uint8_t *buf, size_t buf_len, uint8_t *out,
				      size_t max_len, size_t *out_len);

/**
 * @brief Parse a measurement payload string into its five raw fields
 *
 * @param response  NUL-terminated payload (as produced by extract_frame)
 * @param sensor_id Output sensor id
 * @param timestamp Output probe timestamp
 * @param co2_raw   Output CO2 (Vol.-% * 1000, may be a negative error code)
 * @param temp_raw  Output temperature (degC * 10)
 * @param pressure  Output pressure (mbar)
 * @retval 0 on success
 * @retval -EINVAL if a pointer argument is NULL or fewer than 5 fields parse
 */
int drv_microsens_180hs_parse_measurement(const char *response, uint32_t *sensor_id,
					  uint32_t *timestamp, int32_t *co2_raw,
					  int16_t *temp_raw, int16_t *pressure);

#ifdef __cplusplus
}
#endif

#endif /* DRV_MICROSENS_180HS_PROTO_H_ */
