/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/** @file
 *  @brief Unit tests for the pure Promag MF7S card-frame parser.
 *
 *  Runs on native_sim, no UART involved. Covers UID hex decode, the STX+8hex+CR+LF+ETX
 *  frame structure, partial-frame handling and the consumed-bytes bookkeeping the
 *  listener relies on to preserve a partial trailing frame.
 */

#include <zephyr/ztest.h>

#include <errno.h>
#include <string.h>

#include <drv_promag_mf7s_proto.h>

#define STX DRV_PROMAG_MF7S_STX
#define ETX DRV_PROMAG_MF7S_ETX
#define CR  DRV_PROMAG_MF7S_CR
#define LF  DRV_PROMAG_MF7S_LF

/* A well-formed frame carrying the ASCII UID "ABCD0123" -> 0xABCD0123. */
static const uint8_t GOOD[] = {STX, 'A', 'B', 'C', 'D', '0', '1', '2', '3', CR, LF, ETX};

/* ---- parse_uid ----------------------------------------------------------------- */

ZTEST(promag_proto, test_parse_uid_ok)
{
	uint32_t uid = 0;

	zassert_ok(drv_promag_mf7s_parse_uid((const uint8_t *)"ABCD0123", &uid));
	zassert_equal(uid, 0xABCD0123);
}

ZTEST(promag_proto, test_parse_uid_lowercase)
{
	uint32_t uid = 0;

	zassert_ok(drv_promag_mf7s_parse_uid((const uint8_t *)"deadbeef", &uid));
	zassert_equal(uid, 0xDEADBEEF);
}

ZTEST(promag_proto, test_parse_uid_rejects_non_hex)
{
	uint32_t uid = 0;

	zassert_equal(drv_promag_mf7s_parse_uid((const uint8_t *)"ABCD012G", &uid), -EINVAL);
}

ZTEST(promag_proto, test_parse_uid_rejects_null)
{
	uint32_t uid = 0;

	zassert_equal(drv_promag_mf7s_parse_uid(NULL, &uid), -EINVAL);
	zassert_equal(drv_promag_mf7s_parse_uid((const uint8_t *)"00000000", NULL), -EINVAL);
}

/* ---- parse_card_frame ---------------------------------------------------------- */

ZTEST(promag_proto, test_frame_ok)
{
	uint32_t uid = 0;
	size_t consumed = 0;

	zassert_ok(drv_promag_mf7s_parse_card_frame(GOOD, sizeof(GOOD), &uid, &consumed));
	zassert_equal(uid, 0xABCD0123);
	zassert_equal(consumed, DRV_PROMAG_MF7S_CARD_FRAME_LEN);
}

ZTEST(promag_proto, test_frame_with_leading_garbage)
{
	uint8_t buf[4 + sizeof(GOOD)];
	uint32_t uid = 0;
	size_t consumed = 0;

	memset(buf, 0xAA, 4); /* junk before STX */
	memcpy(&buf[4], GOOD, sizeof(GOOD));

	zassert_ok(drv_promag_mf7s_parse_card_frame(buf, sizeof(buf), &uid, &consumed));
	zassert_equal(uid, 0xABCD0123);
	/* consumed = offset of STX + full frame */
	zassert_equal(consumed, 4 + DRV_PROMAG_MF7S_CARD_FRAME_LEN);
}

ZTEST(promag_proto, test_frame_bad_terminator)
{
	uint8_t buf[sizeof(GOOD)];
	uint32_t uid = 0;
	size_t consumed = 0;

	memcpy(buf, GOOD, sizeof(GOOD));
	buf[11] = 0x00; /* ETX corrupted */

	zassert_equal(drv_promag_mf7s_parse_card_frame(buf, sizeof(buf), &uid, &consumed), -EAGAIN);
}

ZTEST(promag_proto, test_frame_non_hex_body)
{
	uint8_t buf[sizeof(GOOD)];
	uint32_t uid = 0;
	size_t consumed = 0;

	memcpy(buf, GOOD, sizeof(GOOD));
	buf[3] = 'Z'; /* not hex */

	zassert_equal(drv_promag_mf7s_parse_card_frame(buf, sizeof(buf), &uid, &consumed), -EAGAIN);
}

ZTEST(promag_proto, test_frame_incomplete_preserves_partial)
{
	/* STX present but frame not fully arrived: keep the partial from STX onward. */
	uint8_t buf[6];
	uint32_t uid = 0;
	size_t consumed = 0;

	memset(buf, 0x11, 2);
	buf[2] = STX;
	buf[3] = 'A';
	buf[4] = 'B';
	buf[5] = 'C';

	zassert_equal(drv_promag_mf7s_parse_card_frame(buf, sizeof(buf), &uid, &consumed), -EAGAIN);
	zassert_equal(consumed, 2, "drop only the bytes before STX");
}

ZTEST(promag_proto, test_frame_trailing_stx_kept)
{
	/* No frame, but a lone STX at the end must be kept for the next read. */
	uint8_t buf[5] = {0x10, 0x20, 0x30, 0x40, STX};
	uint32_t uid = 0;
	size_t consumed = 0;

	zassert_equal(drv_promag_mf7s_parse_card_frame(buf, sizeof(buf), &uid, &consumed), -EAGAIN);
	zassert_equal(consumed, 4, "keep the trailing STX");
}

ZTEST(promag_proto, test_frame_rejects_null)
{
	uint32_t uid = 0;
	size_t consumed = 0;

	zassert_equal(drv_promag_mf7s_parse_card_frame(NULL, 12, &uid, &consumed), -EINVAL);
	zassert_equal(drv_promag_mf7s_parse_card_frame(GOOD, sizeof(GOOD), NULL, &consumed),
		      -EINVAL);
	zassert_equal(drv_promag_mf7s_parse_card_frame(GOOD, sizeof(GOOD), &uid, NULL), -EINVAL);
}

ZTEST_SUITE(promag_proto, NULL, NULL, NULL, NULL, NULL);
