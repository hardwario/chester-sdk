/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_microsens_180hs_proto.h"

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int drv_microsens_180hs_extract_frame(const uint8_t *buf, size_t buf_len, uint8_t *out,
				      size_t max_len, size_t *out_len)
{
	if (buf == NULL || out == NULL || out_len == NULL) {
		return -EINVAL;
	}

	const uint8_t *stx = memchr(buf, DRV_MICROSENS_180HS_STX, buf_len);
	if (stx == NULL) {
		return -EIO;
	}

	const uint8_t *etx = memchr(stx + 1, DRV_MICROSENS_180HS_ETX,
				    buf_len - (size_t)(stx - buf) - 1);
	if (etx == NULL) {
		return -EIO;
	}

	size_t data_len = (size_t)(etx - stx - 1);
	if (data_len > max_len) {
		return -ENOMEM;
	}

	memcpy(out, stx + 1, data_len);
	out[data_len] = '\0';
	*out_len = data_len;

	return 0;
}

int drv_microsens_180hs_parse_measurement(const char *response, uint32_t *sensor_id,
					  uint32_t *timestamp, int32_t *co2_raw,
					  int16_t *temp_raw, int16_t *pressure)
{
	if (response == NULL || sensor_id == NULL || timestamp == NULL || co2_raw == NULL ||
	    temp_raw == NULL || pressure == NULL) {
		return -EINVAL;
	}

	int parsed = sscanf(response, "%u %u %d %hd %hd", sensor_id, timestamp, co2_raw, temp_raw,
			    pressure);

	if (parsed != 5) {
		return -EINVAL;
	}

	return 0;
}
