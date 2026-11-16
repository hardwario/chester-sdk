/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_msg.h"

/* HIO includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/ztest.h>

/* Third-party includes */
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

#define HASH      0x0102030405060708ULL
#define HDR_LEN   9
#define GUARD_LEN 16
#define GUARD     0xa5

/* Backing memory with a guard area behind the buffer the code under test sees */
static uint8_t m_mem[HDR_LEN + 64 + GUARD_LEN];
static struct ctr_buf m_buf;

static const uint8_t m_hdr[HDR_LEN] = {0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static void buf_setup(size_t size)
{
	zassert_true(size + GUARD_LEN <= sizeof(m_mem));
	memset(m_mem, GUARD, sizeof(m_mem));
	zassert_ok(ctr_buf_init(&m_buf, m_mem, size));
}

static void guard_check(void)
{
	for (size_t i = m_buf.size; i < m_buf.size + GUARD_LEN; i++) {
		zassert_equal(m_mem[i], GUARD, "guard byte %zu overwritten", i);
	}
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);
	buf_setup(sizeof(m_mem) - GUARD_LEN);
}

ZTEST_SUITE(ctr_cloud_msg_data, NULL, NULL, before, NULL, NULL);

/* ---- Callbacks ---- */

static int cb_map_1_42(zcbor_state_t *zs, void *user_data)
{
	int *called = user_data;

	if (called) {
		(*called)++;
	}

	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 1);
	zcbor_uint32_put(zs, 42);
	zcbor_map_end_encode(zs, 1);

	return zcbor_peek_error(zs) ? -EIO : 0;
}

static int cb_return(zcbor_state_t *zs, void *user_data)
{
	zcbor_uint32_put(zs, 1);

	return *(int *)user_data;
}

static int cb_nothing(zcbor_state_t *zs, void *user_data)
{
	return 0;
}

/* Ignores the put_raw() result on purpose and still returns 0 */
static int cb_ignore_put_raw_error(zcbor_state_t *zs, void *user_data)
{
	uint8_t big[100] = {0};

	(void)ctr_cloud_cbor_put_raw(zs, big, sizeof(big));

	return 0;
}

struct stop_ctx {
	const uint8_t *before;
	const uint8_t *after;
	int err;
};

static int cb_stop_on_error(zcbor_state_t *zs, void *user_data)
{
	struct stop_ctx *ctx = user_data;
	uint8_t big[100] = {0};

	(void)ctr_cloud_cbor_put_raw(zs, big, sizeof(big));

	ctx->before = zs->payload;
	zcbor_uint32_put(zs, 7); /* would fit, but the state is in error */
	ctx->after = zs->payload;
	ctx->err = zcbor_peek_error(zs);

	return 0;
}

static int cb_other_error(zcbor_state_t *zs, void *user_data)
{
	zcbor_uint32_put(zs, 1);
	zcbor_error(zs, ZCBOR_ERR_ASSERTION);

	return 0;
}

/* put_raw() of exactly cbor_free() + *delta bytes */
static int cb_fill(zcbor_state_t *zs, void *user_data)
{
	int delta = *(int *)user_data;
	size_t n = ctr_cloud_cbor_free(zs) + delta;
	static uint8_t raw[HDR_LEN + 64 + 2];

	memset(raw, 0x01, sizeof(raw));
	zassert_true(n <= sizeof(raw));

	int ret = ctr_cloud_cbor_put_raw(zs, raw, n);

	return delta > 0 ? 0 : ret;
}

/* ---- pack_data ---- */

ZTEST(ctr_cloud_msg_data, test_pack_data_bytes)
{
	static const uint8_t payload[] = {0xa1, 0x01, 0x02};
	struct ctr_buf ref;
	uint8_t ref_mem[HDR_LEN + sizeof(payload)];

	zassert_ok(ctr_cloud_msg_pack_data(&m_buf, HASH, payload, sizeof(payload)));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + sizeof(payload));
	zassert_mem_equal(m_mem, m_hdr, HDR_LEN);

	/* Same bytes as ctr_cloud_send_data() built by hand before */
	zassert_ok(ctr_buf_init(&ref, ref_mem, sizeof(ref_mem)));
	zassert_ok(ctr_buf_append_u8(&ref, 0x06));
	zassert_ok(ctr_buf_append_u64_be(&ref, HASH));
	zassert_ok(ctr_buf_append_mem(&ref, payload, sizeof(payload)));
	zassert_mem_equal(m_mem, ref_mem, sizeof(ref_mem));
}

ZTEST(ctr_cloud_msg_data, test_pack_data_appends)
{
	static const uint8_t payload[] = {0xf6};

	zassert_ok(ctr_buf_append_u8(&m_buf, 0x55));
	zassert_ok(ctr_cloud_msg_pack_data(&m_buf, HASH, payload, sizeof(payload)));
	zassert_equal(ctr_buf_get_used(&m_buf), 1 + HDR_LEN + sizeof(payload));
	zassert_equal(m_mem[0], 0x55);
	zassert_mem_equal(m_mem + 1, m_hdr, HDR_LEN);
	zassert_equal(m_mem[1 + HDR_LEN], 0xf6);
}

ZTEST(ctr_cloud_msg_data, test_pack_data_nospc)
{
	static const uint8_t payload[] = {0xf6};

	buf_setup(HDR_LEN - 1);
	zassert_equal(ctr_cloud_msg_pack_data(&m_buf, HASH, payload, sizeof(payload)), -ENOSPC);
	guard_check();

	buf_setup(HDR_LEN);
	zassert_equal(ctr_cloud_msg_pack_data(&m_buf, HASH, payload, sizeof(payload)), -ENOSPC);
	guard_check();
}

/* ---- pack_data_cb ---- */

ZTEST(ctr_cloud_msg_data, test_encode_exact_bytes)
{
	static const uint8_t expect[] = {0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					 0x07, 0x08, 0xbf, 0x01, 0x18, 0x2a, 0xff};
	int called = 0;

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_map_1_42, &called));
	zassert_equal(called, 1);
	zassert_equal(ctr_buf_get_used(&m_buf), sizeof(expect));
	zassert_mem_equal(m_mem, expect, sizeof(expect));
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_args)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, NULL, NULL), -EINVAL);
	zassert_equal(ctr_cloud_msg_pack_data_cb(NULL, HASH, cb_map_1_42, NULL), -EINVAL);
}

ZTEST(ctr_cloud_msg_data, test_encode_cb_error_passthrough)
{
	int ret = -EIO;

	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_return, &ret), -EIO);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);

	ret = -ENOMEM;
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_return, &ret), -ENOMEM);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

ZTEST(ctr_cloud_msg_data, test_encode_cb_positive_rejected)
{
	/* Positive values are reserved for the cloud subsystem */
	int ret = 5;

	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_return, &ret), -EINVAL);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

ZTEST(ctr_cloud_msg_data, test_encode_ignored_put_raw_error)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_ignore_put_raw_error, NULL),
		      -ENOSPC);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_stop_on_error)
{
	struct stop_ctx ctx = {0};

	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_stop_on_error, &ctx), -ENOSPC);
	zassert_equal(ctx.err, ZCBOR_ERR_NO_PAYLOAD);
	zassert_equal(ctx.before, ctx.after, "write happened after an error");
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

ZTEST(ctr_cloud_msg_data, test_encode_other_zcbor_error)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_other_error, NULL), -EBADMSG);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

ZTEST(ctr_cloud_msg_data, test_encode_empty_payload)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_nothing, NULL), -EINVAL);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

ZTEST(ctr_cloud_msg_data, test_encode_boundary_exact_fit)
{
	int delta = 0;

	buf_setup(HDR_LEN + 16);
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_fill, &delta));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + 16);
	zassert_equal(ctr_buf_get_free(&m_buf), 0);
	zassert_mem_equal(m_mem, m_hdr, HDR_LEN);
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_boundary_one_over)
{
	int delta = 1;

	buf_setup(HDR_LEN + 16);
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_fill, &delta), -ENOSPC);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_header_not_overwritten_no_overflow)
{
	int delta = 0;

	buf_setup(HDR_LEN + 8);
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_fill, &delta));
	zassert_mem_equal(m_mem, m_hdr, HDR_LEN);
	guard_check();

	delta = 5;
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_fill, &delta), -ENOSPC);
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_small_buffer_nospc)
{
	int called = 0;

	/* Header does not fit: callback not called, the partial header is rolled back */
	buf_setup(HDR_LEN - 1);
	zassert_ok(ctr_buf_seek(&m_buf, 3));
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_map_1_42, &called), -ENOSPC);
	zassert_equal(called, 0);
	zassert_equal(ctr_buf_get_used(&m_buf), 3);
	guard_check();
}

ZTEST(ctr_cloud_msg_data, test_encode_reusable_after_failure)
{
	int delta = 1;
	int called = 0;

	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_fill, &delta), -ENOSPC);
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_map_1_42, &called));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + 5);
	zassert_equal(m_mem[HDR_LEN], 0xbf);
}

ZTEST(ctr_cloud_msg_data, test_encode_appends)
{
	int called = 0;

	zassert_ok(ctr_buf_append_u8(&m_buf, 0x55));
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_map_1_42, &called));
	zassert_equal(ctr_buf_get_used(&m_buf), 1 + HDR_LEN + 5);
	zassert_equal(m_mem[0], 0x55);
	zassert_mem_equal(m_mem + 1, m_hdr, HDR_LEN);
	zassert_equal(m_mem[1 + HDR_LEN], 0xbf);
}

ZTEST(ctr_cloud_msg_data, test_encode_failure_keeps_previous_content)
{
	int ret = -EIO;

	zassert_ok(ctr_buf_append_u8(&m_buf, 0x55));
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_return, &ret), -EIO);
	zassert_equal(ctr_buf_get_used(&m_buf), 1);
	zassert_equal(m_mem[0], 0x55);
}

static uint8_t m_big_mem[16384];
static uint8_t m_big_raw[16000];

static int cb_big(zcbor_state_t *zs, void *user_data)
{
	return ctr_cloud_cbor_put_raw(zs, m_big_raw, sizeof(m_big_raw));
}

ZTEST(ctr_cloud_msg_data, test_encode_large_payload)
{
	struct ctr_buf big;

	memset(m_big_raw, 0x42, sizeof(m_big_raw));
	zassert_ok(ctr_buf_init(&big, m_big_mem, sizeof(m_big_mem)));
	zassert_ok(ctr_cloud_msg_pack_data_cb(&big, HASH, cb_big, NULL));
	zassert_equal(ctr_buf_get_used(&big), HDR_LEN + sizeof(m_big_raw));
	zassert_mem_equal(m_big_mem, m_hdr, HDR_LEN);
	zassert_mem_equal(&m_big_mem[HDR_LEN], m_big_raw, sizeof(m_big_raw));
}

/* ---- cbor_free / cbor_len ---- */

struct fl_ctx {
	size_t free0, len0, free1, len1;
};

static int cb_free_len(zcbor_state_t *zs, void *user_data)
{
	struct fl_ctx *c = user_data;

	c->free0 = ctr_cloud_cbor_free(zs);
	c->len0 = ctr_cloud_cbor_len(zs);
	cb_map_1_42(zs, NULL);
	c->free1 = ctr_cloud_cbor_free(zs);
	c->len1 = ctr_cloud_cbor_len(zs);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_free_len_basic)
{
	struct fl_ctx c = {0};

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_free_len, &c));
	zassert_equal(c.len0, 0);
	zassert_equal(c.free0, m_buf.size - HDR_LEN);
	zassert_equal(c.len1, 5);
	zassert_equal(c.free1, m_buf.size - HDR_LEN - 5);
}

struct nest_ctx {
	size_t len[8];
	size_t free[8];
};

static int cb_nested_len(zcbor_state_t *zs, void *user_data)
{
	struct nest_ctx *c = user_data;
	int i = 0;

#define SNAP()                                                                                     \
	do {                                                                                       \
		c->len[i] = ctr_cloud_cbor_len(zs);                                                \
		c->free[i] = ctr_cloud_cbor_free(zs);                                              \
		i++;                                                                               \
	} while (0)

	zcbor_map_start_encode(zs, 1); /* BF */
	SNAP();                        /* 1 B */
	zcbor_uint32_put(zs, 1);
	zcbor_list_start_encode(zs, 2); /* 9F */
	SNAP();                         /* 1 + 1 + 1 = 3 B */
	zcbor_map_start_encode(zs, 1);  /* BF */
	zcbor_uint32_put(zs, 2);
	zcbor_tstr_put_lit(zs, "ab"); /* 1 + 3 */
	SNAP();                       /* 3 + 1 + 1 + 3 = 8 */
	zcbor_map_end_encode(zs, 1);  /* FF */
	zcbor_list_end_encode(zs, 2); /* FF */
	SNAP();                       /* 10 */
	zcbor_map_end_encode(zs, 1);  /* FF */
	SNAP();                       /* 11 */
#undef SNAP

	return zcbor_peek_error(zs) ? -EIO : 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_free_len_nested)
{
	struct nest_ctx c = {0};
	static const size_t expect_len[] = {1, 3, 8, 10, 11};

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_nested_len, &c));
	for (int i = 0; i < ARRAY_SIZE(expect_len); i++) {
		zassert_equal(c.len[i], expect_len[i], "len[%d] = %zu", i, c.len[i]);
		zassert_equal(c.len[i] + c.free[i], m_buf.size - HDR_LEN, "free[%d]", i);
	}
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + 11);
}

/* ---- cbor_put_raw ---- */

struct raw_ctx {
	size_t elem_before, elem_after;
	int ret;
	int ret2;
	int err;
	const uint8_t *p_before, *p_after;
};

static const uint8_t m_raw_list[] = {0x9f, 0x01, 0x02, 0xff};

static int cb_put_raw_value(zcbor_state_t *zs, void *user_data)
{
	struct raw_ctx *c = user_data;

	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 5);
	c->elem_before = zs->elem_count;
	c->ret = ctr_cloud_cbor_put_raw(zs, m_raw_list, sizeof(m_raw_list));
	c->elem_after = zs->elem_count;
	zcbor_map_end_encode(zs, 1);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_raw_value)
{
	static const uint8_t expect[] = {0xbf, 0x05, 0x9f, 0x01, 0x02, 0xff, 0xff};
	struct raw_ctx c = {0};
	zcbor_state_t zd[4];
	uint32_t key = 0, a = 0, b = 0;

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_value, &c));
	zassert_ok(c.ret);
	zassert_equal(c.elem_after, c.elem_before + 1);
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + sizeof(expect));
	zassert_mem_equal(&m_mem[HDR_LEN], expect, sizeof(expect));

	/* Decodable */
	zcbor_new_decode_state(zd, ARRAY_SIZE(zd), &m_mem[HDR_LEN], sizeof(expect), 1, NULL, 0);
	zassert_true(zcbor_map_start_decode(zd));
	zassert_true(zcbor_uint32_decode(zd, &key));
	zassert_equal(key, 5);
	zassert_true(zcbor_list_start_decode(zd));
	zassert_true(zcbor_uint32_decode(zd, &a));
	zassert_true(zcbor_uint32_decode(zd, &b));
	zassert_true(zcbor_list_end_decode(zd));
	zassert_true(zcbor_map_end_decode(zd));
	zassert_equal(a, 1);
	zassert_equal(b, 2);
}

static int cb_put_raw_nospc(zcbor_state_t *zs, void *user_data)
{
	struct raw_ctx *c = user_data;
	static uint8_t big[100];

	c->p_before = zs->payload;
	c->elem_before = zs->elem_count;
	c->ret = ctr_cloud_cbor_put_raw(zs, big, sizeof(big));
	c->p_after = zs->payload;
	c->elem_after = zs->elem_count;
	c->err = zcbor_peek_error(zs);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_raw_nospc)
{
	struct raw_ctx c = {0};

	buf_setup(HDR_LEN + 50);
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_nospc, &c), -ENOSPC);
	zassert_equal(c.ret, -ENOSPC);
	zassert_equal(c.err, ZCBOR_ERR_NO_PAYLOAD);
	zassert_equal(c.p_before, c.p_after);
	zassert_equal(c.elem_before, c.elem_after);
	guard_check();
}

static int cb_put_raw_args(zcbor_state_t *zs, void *user_data)
{
	struct raw_ctx *c = user_data;
	uint8_t one = 0x01;

	c->p_before = zs->payload;
	c->ret = ctr_cloud_cbor_put_raw(zs, NULL, 1);
	c->err = zcbor_peek_error(zs);
	c->p_after = zs->payload;
	c->ret2 = ctr_cloud_cbor_put_raw(NULL, &one, 1);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_raw_invalid_args)
{
	struct raw_ctx c = {0};

	/* Nothing is written, but the state fails, so ignoring the return value is caught */
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_args, &c), -EBADMSG);
	zassert_equal(c.ret, -EINVAL);
	zassert_equal(c.err, ZCBOR_ERR_WRONG_VALUE);
	zassert_equal(c.p_before, c.p_after);
	zassert_equal(c.ret2, -EINVAL);
}

static int cb_put_raw_zero_len_ignored(zcbor_state_t *zs, void *user_data)
{
	uint8_t one = 0x01;

	zcbor_map_start_encode(zs, 1);
	zcbor_uint32_put(zs, 1);
	(void)ctr_cloud_cbor_put_raw(zs, &one, 0); /* ignored */
	zcbor_map_end_encode(zs, 1);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_encode_put_raw_zero_len_ignored)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_zero_len_ignored, NULL),
		      -EBADMSG);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}

static int cb_put_raw_after_error(zcbor_state_t *zs, void *user_data)
{
	struct raw_ctx *c = user_data;
	uint8_t one = 0x01;

	zcbor_error(zs, ZCBOR_ERR_ASSERTION);
	c->p_before = zs->payload;
	c->ret = ctr_cloud_cbor_put_raw(zs, &one, 1);
	c->p_after = zs->payload;
	c->err = zcbor_peek_error(zs);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_raw_after_error)
{
	struct raw_ctx c = {0};

	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_after_error, &c),
		      -EBADMSG);
	zassert_equal(c.ret, -EBADMSG);
	zassert_equal(c.p_before, c.p_after);
	zassert_equal(c.err, ZCBOR_ERR_ASSERTION);
}

/* ---- cbor_put_key_ts ---- */

struct ts_ctx {
	uint32_t key;
	int64_t ts;
};

static int cb_key_ts(zcbor_state_t *zs, void *user_data)
{
	struct ts_ctx *c = user_data;

	zcbor_map_start_encode(zs, 1);
	ctr_cloud_cbor_put_key_ts(zs, c->key, c->ts);
	zcbor_map_end_encode(zs, 1);

	return zcbor_peek_error(zs) ? -EIO : 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_key_ts_zero_is_null)
{
	static const uint8_t expect[] = {0xbf, 0x03, 0xf6, 0xff};
	struct ts_ctx c = {.key = 3, .ts = 0};

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_key_ts, &c));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + sizeof(expect));
	zassert_mem_equal(&m_mem[HDR_LEN], expect, sizeof(expect));
}

static void check_ts_roundtrip(int64_t ts)
{
	struct ts_ctx c = {.key = 3, .ts = ts};
	zcbor_state_t zd[4];
	uint32_t key = 0;
	int64_t out = 0;

	/* Called several times per test; pack_data_cb appends like the other pack functions */
	ctr_buf_reset(&m_buf);
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_key_ts, &c));
	zcbor_new_decode_state(zd, ARRAY_SIZE(zd), &m_mem[HDR_LEN],
			       ctr_buf_get_used(&m_buf) - HDR_LEN, 1, NULL, 0);
	zassert_true(zcbor_map_start_decode(zd));
	zassert_true(zcbor_uint32_decode(zd, &key));
	zassert_true(zcbor_int64_decode(zd, &out));
	zassert_true(zcbor_map_end_decode(zd));
	zassert_equal(key, 3);
	zassert_equal(out, ts);
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_key_ts_value)
{
	static const uint8_t expect[] = {0xbf, 0x03, 0x1a, 0x65, 0x53, 0xf1, 0x00, 0xff};
	struct ts_ctx c = {.key = 3, .ts = 1700000000};

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_key_ts, &c));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + sizeof(expect));
	zassert_mem_equal(&m_mem[HDR_LEN], expect, sizeof(expect));

	check_ts_roundtrip(1);
	check_ts_roundtrip(INT64_C(5000000000));
	check_ts_roundtrip(-1700000000);
}

static int cb_key_ts_nospc(zcbor_state_t *zs, void *user_data)
{
	struct raw_ctx *c = user_data;

	c->p_before = zs->payload;
	ctr_cloud_cbor_put_key_ts(zs, 3, 1700000000);
	c->p_after = zs->payload;
	c->err = zcbor_peek_error(zs);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_cbor_put_key_ts_nospc)
{
	struct raw_ctx c = {0};

	/* Key (1 B) fits, the 5 B timestamp does not */
	buf_setup(HDR_LEN + 3);
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_key_ts_nospc, &c), -ENOSPC);
	zassert_equal(c.err, ZCBOR_ERR_NO_PAYLOAD);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
	guard_check();
}

/* ---- put_raw after an overflow inside pack_data_cb ---- */

static int cb_put_raw_after_overflow(zcbor_state_t *zs, void *user_data)
{
	static uint8_t big[100];
	uint8_t one = 0x01;

	(void)ctr_cloud_cbor_put_raw(zs, big, sizeof(big)); /* overflow, ignored */

	return ctr_cloud_cbor_put_raw(zs, &one, 1);
}

ZTEST(ctr_cloud_msg_data, test_encode_put_raw_after_overflow)
{
	buf_setup(HDR_LEN + 50);
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_put_raw_after_overflow, NULL),
		      -ENOSPC);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
	guard_check();
}

/* ---- Nested encode, foreign state, no encode ---- */

struct nested_ctx {
	size_t len_before;
	size_t len_after;
	size_t foreign;
	int inner_ret;
	zcbor_state_t saved;
};

static uint8_t m_inner_mem[64];

static int cb_nested_encode(zcbor_state_t *zs, void *user_data)
{
	struct nested_ctx *c = user_data;
	struct ctr_buf inner;
	zcbor_state_t app[3];
	uint8_t app_mem[16];

	zcbor_uint32_put(zs, 1);
	c->len_before = ctr_cloud_cbor_len(zs);

	/* Application-owned state during the active encode */
	zcbor_new_encode_state(app, ARRAY_SIZE(app), app_mem, sizeof(app_mem), 1);
	zcbor_uint32_put(app, 1);
	zcbor_uint32_put(app, 2);
	c->foreign = ctr_cloud_cbor_len(app);

	/* Nested (forbidden) encode into another buffer must not disturb this one */
	zassert_ok(ctr_buf_init(&inner, m_inner_mem, sizeof(m_inner_mem)));
	c->inner_ret = ctr_cloud_msg_pack_data_cb(&inner, HASH, cb_map_1_42, NULL);

	c->len_after = ctr_cloud_cbor_len(zs);
	c->saved = *zs;

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_encode_nested_keeps_outer_len)
{
	struct nested_ctx c = {0};

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_nested_encode, &c));
	zassert_ok(c.inner_ret);
	zassert_equal(c.len_before, 1);
	zassert_equal(c.len_after, 1);
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + 1);
}

ZTEST(ctr_cloud_msg_data, test_cbor_len_foreign_state)
{
	struct nested_ctx c = {0};

	c.foreign = 99;
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_nested_encode, &c));
	zassert_equal(c.foreign, 0);
}

ZTEST(ctr_cloud_msg_data, test_cbor_len_outside_encode)
{
	struct nested_ctx c = {0};
	zcbor_state_t app[3];
	uint8_t app_mem[16];

	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_nested_encode, &c));

	/* The state of a finished encode and an application state: both 0 */
	zassert_equal(ctr_cloud_cbor_len(&c.saved), 0);

	zcbor_new_encode_state(app, ARRAY_SIZE(app), app_mem, sizeof(app_mem), 1);
	zcbor_uint32_put(app, 1);
	zassert_equal(ctr_cloud_cbor_len(app), 0);
}

/* ---- Default number of zcbor backup states ---- */

#if CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES == 2
/* Opens map, list, map, list, ... and closes them in reverse: list_end, map_end, ... */
static int cb_deep(zcbor_state_t *zs, void *user_data)
{
	for (int i = 0; i < 3; i++) {
		zcbor_map_start_encode(zs, 1);
		zcbor_uint32_put(zs, i);
		zcbor_list_start_encode(zs, 1);
	}
	zcbor_uint32_put(zs, 9);
	for (int i = 0; i < 3; i++) {
		zcbor_list_end_encode(zs, 1);
		zcbor_map_end_encode(zs, 1);
	}

	return zcbor_peek_error(zs) ? -EIO : 0;
}

ZTEST(ctr_cloud_msg_data, test_default_states_deep_nesting)
{
	static const uint8_t expect[] = {0xbf, 0x00, 0x9f, 0xbf, 0x01, 0x9f, 0xbf, 0x02,
					 0x9f, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	zassert_equal(CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES, 2);
	zassert_ok(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_deep, NULL));
	zassert_equal(ctr_buf_get_used(&m_buf), HDR_LEN + sizeof(expect));
	zassert_mem_equal(&m_mem[HDR_LEN], expect, sizeof(expect));
}
#endif

#if CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES == 0
static int cb_bstr_wrap(zcbor_state_t *zs, void *user_data)
{
	/* Needs one backup state */
	(void)zcbor_bstr_start_encode(zs);

	return 0;
}

ZTEST(ctr_cloud_msg_data, test_encode_no_backup_states_nomem)
{
	zassert_equal(ctr_cloud_msg_pack_data_cb(&m_buf, HASH, cb_bstr_wrap, NULL), -ENOMEM);
	zassert_equal(ctr_buf_get_used(&m_buf), 0);
}
#endif
