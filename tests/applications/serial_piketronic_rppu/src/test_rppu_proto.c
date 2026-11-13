/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/** @file
 *  @brief Unit tests for the pure Piketronic RPP-U frame codec.
 *
 *  Runs on native_sim, no UART / SC16IS740 involved. Guards the checksum, the frame
 *  validation and the DATA payload field offsets - the byte-level logic that used to be
 *  verifiable only against real probe hardware.
 *
 *  west twister -p native_sim -c -i --testsuite-root chester/tests -T tests/applications/serial_piketronic_rppu
 */

#include <zephyr/ztest.h>

#include <errno.h>
#include <string.h>

#include <drv_piketronic_rppu_proto.h>

#define FRAME_LEN DRV_PIKETRONIC_RPPU_DATA_FRAME_LEN /* 66 */

/* Build a well-formed DATA reply around a 61-byte payload: '@' nB_H nB_L 'D' payload CSUM. */
static void build_data_frame(const uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN],
			     uint8_t out[FRAME_LEN])
{
	/* nB = Cmd + data = 1 + 61 = 62 */
	const uint16_t nb = 1 + DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN;

	out[0] = DRV_PIKETRONIC_RPPU_FRAME_START;
	out[1] = (uint8_t)(nb >> 8);
	out[2] = (uint8_t)(nb & 0xff);
	out[3] = 0x44; /* 'D' */
	memcpy(&out[4], payload, DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN);
	out[FRAME_LEN - 1] = drv_piketronic_rppu_checksum(&out[1], FRAME_LEN - 2);
}

/* ---- checksum ------------------------------------------------------------------ */

ZTEST(rppu_proto, test_checksum_vendor_examples)
{
	/* The two request frames ship with vendor-verified checksums:
	 * CMD_DATA   = 40 00 01 44 45 -> csum over {00,01,44} = 0x45
	 * CMD_SERIAL = 40 00 01 56 57 -> csum over {00,01,56} = 0x57
	 */
	const uint8_t data_req[] = {0x00, 0x01, 0x44};
	const uint8_t serial_req[] = {0x00, 0x01, 0x56};

	zassert_equal(drv_piketronic_rppu_checksum(data_req, sizeof(data_req)), 0x45);
	zassert_equal(drv_piketronic_rppu_checksum(serial_req, sizeof(serial_req)), 0x57);
}

ZTEST(rppu_proto, test_checksum_wraps_to_low_byte)
{
	/* 0x80 + 0x80 + 0x01 = 0x101 -> low byte 0x01 */
	const uint8_t buf[] = {0x80, 0x80, 0x01};

	zassert_equal(drv_piketronic_rppu_checksum(buf, sizeof(buf)), 0x01);
}

ZTEST(rppu_proto, test_checksum_empty_is_zero)
{
	zassert_equal(drv_piketronic_rppu_checksum((const uint8_t *)"", 0), 0x00);
}

/* ---- verify_frame -------------------------------------------------------------- */

ZTEST(rppu_proto, test_verify_frame_accepts_valid)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];

	memset(payload, 0xA5, sizeof(payload));
	build_data_frame(payload, frame);

	zassert_ok(drv_piketronic_rppu_verify_frame(frame, FRAME_LEN));
}

ZTEST(rppu_proto, test_verify_frame_rejects_null_and_short)
{
	uint8_t frame[FRAME_LEN] = {0};

	zassert_equal(drv_piketronic_rppu_verify_frame(NULL, FRAME_LEN), -EINVAL);
	zassert_equal(drv_piketronic_rppu_verify_frame(frame, 4), -EINVAL);
}

ZTEST(rppu_proto, test_verify_frame_rejects_bad_start)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];

	memset(payload, 0, sizeof(payload));
	build_data_frame(payload, frame);
	frame[0] = 0x23; /* not '@' */

	zassert_equal(drv_piketronic_rppu_verify_frame(frame, FRAME_LEN), -EBADMSG);
}

ZTEST(rppu_proto, test_verify_frame_rejects_bad_nb)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];

	memset(payload, 0, sizeof(payload));
	build_data_frame(payload, frame);
	frame[2] ^= 0x01; /* corrupt nB_L; checksum recomputation below keeps CSUM valid */
	frame[FRAME_LEN - 1] = drv_piketronic_rppu_checksum(&frame[1], FRAME_LEN - 2);

	zassert_equal(drv_piketronic_rppu_verify_frame(frame, FRAME_LEN), -EBADMSG);
}

ZTEST(rppu_proto, test_verify_frame_rejects_bad_checksum)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];

	memset(payload, 0, sizeof(payload));
	build_data_frame(payload, frame);
	frame[FRAME_LEN - 1] ^= 0xff; /* wreck the checksum only */

	zassert_equal(drv_piketronic_rppu_verify_frame(frame, FRAME_LEN), -EBADMSG);
}

/* ---- decode_data --------------------------------------------------------------- */

ZTEST(rppu_proto, test_decode_data_fields)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];

	/* Distinct, MSB-first values at each documented offset. Distinct values mean a
	 * swapped or shifted offset produces a wrong field, not a silent pass.
	 */
	memset(payload, 0, sizeof(payload));
	payload[0] = 0x11; payload[1] = 0x22; payload[2] = 0x33; payload[3] = 0x44; /* real_time */
	payload[4] = 0xFB;                                                          /* temp = -5 */
	payload[5] = 55;                                                            /* humidity */
	payload[6] = 0x00; payload[7] = 0x00; payload[8] = 0xAB; payload[9] = 0xCD; /* conc_a */
	payload[10] = 0x00; payload[11] = 0x01; payload[12] = 0x23; payload[13] = 0x45; /* conc_ac */
	payload[14] = 0xFF; payload[15] = 0xFF; payload[16] = 0xFF; payload[17] = 0xFF; /* reserved */
	payload[18] = 0x00; payload[19] = 0x00; payload[20] = 0x99; payload[21] = 0x99; /* day_ac */
	payload[22] = 0x01; payload[23] = 0x02;                                         /* conc_time */
	payload[24] = 0x00; payload[25] = 0x00; payload[26] = 0x00; payload[27] = 0x01; /* sum1 */
	payload[28] = 0x00; payload[29] = 0x00; payload[30] = 0x00; payload[31] = 0x02; /* sum2 */
	payload[32] = 0x00; payload[33] = 0x00; payload[34] = 0x00; payload[35] = 0x03; /* sum3 */
	payload[36] = 0x00; payload[37] = 0x00; payload[38] = 0x00; payload[39] = 0x04; /* sum4 */
	payload[40] = 0x07;                                                             /* impulses_hv */
	payload[41] = 0x03; payload[42] = 0x04;                                         /* record_time */
	payload[43] = 0x00; payload[44] = 0x00; payload[45] = 0x00; payload[46] = 0x0A; /* record_count */
	payload[47] = 0x00; payload[48] = 0x00; payload[49] = 0x00; payload[50] = 0x0B; /* spectrum_time */
	payload[51] = 0x05; payload[52] = 0x06;                                         /* spectrum_count */
	payload[53] = 0x00; payload[54] = 0xAB; payload[55] = 0xCD; payload[56] = 0xEF; /* impulses_total */
	payload[57] = 0x01;                                                             /* switch_state */
	payload[58] = 0x0E; payload[59] = 0x10;                                         /* voltage = 3600 */
	payload[60] = 0x00;                                                             /* errors */

	build_data_frame(payload, frame);

	struct drv_piketronic_rppu_data d;
	memset(&d, 0xEE, sizeof(d));

	zassert_ok(drv_piketronic_rppu_decode_data(frame, FRAME_LEN, &d));

	zassert_equal(d.real_time, 0x11223344, "real_time / endianness");
	zassert_equal(d.temperature, -5, "temperature int8");
	zassert_equal(d.humidity, 55);
	zassert_equal(d.concentration_a, 0x0000ABCD);
	zassert_equal(d.concentration_ac, 0x00012345);
	zassert_equal(d.concentration_day_ac, 0x00009999);
	zassert_equal(d.concentration_time, 0x0102);
	zassert_equal(d.sum1, 1);
	zassert_equal(d.sum2, 2);
	zassert_equal(d.sum3, 3);
	zassert_equal(d.sum4, 4);
	zassert_equal(d.impulses_hv, 0x07);
	zassert_equal(d.record_time, 0x0304);
	zassert_equal(d.record_count, 10);
	zassert_equal(d.spectrum_time, 11);
	zassert_equal(d.spectrum_count, 0x0506);
	zassert_equal(d.impulses_total, 0x00ABCDEF);
	zassert_equal(d.switch_state, 1);
	zassert_equal(d.voltage_mv, 3600, "voltage / endianness");
	zassert_equal(d.errors, 0x00);
}

ZTEST(rppu_proto, test_decode_rejects_wrong_length)
{
	/* A serial-number frame (15 bytes) is well-formed but is not a DATA frame. */
	uint8_t serial_frame[DRV_PIKETRONIC_RPPU_SERIAL_FRAME_LEN];

	memset(serial_frame, 0, sizeof(serial_frame));
	serial_frame[0] = DRV_PIKETRONIC_RPPU_FRAME_START;
	serial_frame[1] = 0x00;
	serial_frame[2] = DRV_PIKETRONIC_RPPU_SERIAL_FRAME_LEN - 4;
	serial_frame[3] = 0x56; /* 'V' */
	serial_frame[sizeof(serial_frame) - 1] =
		drv_piketronic_rppu_checksum(&serial_frame[1], sizeof(serial_frame) - 2);

	struct drv_piketronic_rppu_data d;

	zassert_equal(drv_piketronic_rppu_decode_data(serial_frame, sizeof(serial_frame), &d),
		      -EBADMSG);
}

ZTEST(rppu_proto, test_decode_rejects_null)
{
	uint8_t frame[FRAME_LEN] = {0};
	struct drv_piketronic_rppu_data d;

	zassert_equal(drv_piketronic_rppu_decode_data(NULL, FRAME_LEN, &d), -EINVAL);
	zassert_equal(drv_piketronic_rppu_decode_data(frame, FRAME_LEN, NULL), -EINVAL);
}

ZTEST(rppu_proto, test_decode_rejects_corrupt_frame)
{
	uint8_t payload[DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN];
	uint8_t frame[FRAME_LEN];
	struct drv_piketronic_rppu_data d;

	memset(payload, 0, sizeof(payload));
	build_data_frame(payload, frame);
	frame[FRAME_LEN - 1] ^= 0x01; /* break checksum */

	zassert_equal(drv_piketronic_rppu_decode_data(frame, FRAME_LEN, &d), -EBADMSG);
}

ZTEST_SUITE(rppu_proto, NULL, NULL, NULL, NULL, NULL);
