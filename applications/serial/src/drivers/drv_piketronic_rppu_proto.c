/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_piketronic_rppu_proto.h"

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t be16(const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

uint8_t drv_piketronic_rppu_checksum(const uint8_t *after_start, size_t len)
{
	uint32_t sum = 0;

	for (size_t i = 0; i < len; i++) {
		sum += after_start[i];
	}

	return (uint8_t)(sum & 0xff);
}

int drv_piketronic_rppu_verify_frame(const uint8_t *rx, size_t len)
{
	/* Smallest possible frame: '@' nB_H nB_L Cmd CSUM */
	if (rx == NULL || len < 5) {
		return -EINVAL;
	}

	if (rx[0] != DRV_PIKETRONIC_RPPU_FRAME_START) {
		return -EBADMSG;
	}

	/* nB counts Cmd + data: everything between nB_L and CSUM, i.e. len - 4. */
	uint16_t nb = be16(&rx[1]);
	if (nb != len - 4) {
		return -EBADMSG;
	}

	/* CSUM covers everything except the leading '@'. */
	uint8_t want = drv_piketronic_rppu_checksum(&rx[1], len - 2);
	if (want != rx[len - 1]) {
		return -EBADMSG;
	}

	return 0;
}

int drv_piketronic_rppu_decode_data(const uint8_t *frame, size_t len,
				    struct drv_piketronic_rppu_data *out)
{
	if (frame == NULL || out == NULL) {
		return -EINVAL;
	}

	if (len != DRV_PIKETRONIC_RPPU_DATA_FRAME_LEN) {
		return -EBADMSG;
	}

	int ret = drv_piketronic_rppu_verify_frame(frame, len);
	if (ret) {
		return ret;
	}

	/* Field offsets follow the vendor "Actual data get - D" table; d points at the
	 * first payload byte (past '@' nB_H nB_L Cmd).
	 */
	const uint8_t *d = &frame[4];

	out->real_time = be32(&d[0]);
	out->temperature = (int8_t)d[4];
	out->humidity = d[5];
	out->concentration_a = be32(&d[6]);
	out->concentration_ac = be32(&d[10]);
	/* d[14..17] is the reserved field */
	out->concentration_day_ac = be32(&d[18]);
	out->concentration_time = be16(&d[22]);
	out->sum1 = be32(&d[24]);
	out->sum2 = be32(&d[28]);
	out->sum3 = be32(&d[32]);
	out->sum4 = be32(&d[36]);
	out->impulses_hv = d[40];
	out->record_time = be16(&d[41]);
	out->record_count = be32(&d[43]);
	out->spectrum_time = be32(&d[47]);
	out->spectrum_count = be16(&d[51]);
	out->impulses_total = be32(&d[53]);
	out->switch_state = d[57];
	out->voltage_mv = be16(&d[58]);
	out->errors = d[60];

	return 0;
}
