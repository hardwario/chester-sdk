/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Zephyr includes */
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int ctr_buf_init(struct ctr_buf *buf, void *mem, size_t size)
{
	if (!size) {
		return -EINVAL;
	}

	buf->mem = mem;
	buf->size = size;

	ctr_buf_reset(buf);

	return 0;
}

uint8_t *ctr_buf_get_mem(struct ctr_buf *buf)
{
	return buf->mem;
}

size_t ctr_buf_get_free(struct ctr_buf *buf)
{
	return buf->size - buf->len;
}

size_t ctr_buf_get_used(struct ctr_buf *buf)
{
	return buf->len;
}

void ctr_buf_reset(struct ctr_buf *buf)
{
	buf->len = 0;
}

void ctr_buf_fill(struct ctr_buf *buf, int val)
{
	memset(buf->mem, val, buf->size);

	ctr_buf_reset(buf);
}

int ctr_buf_seek(struct ctr_buf *buf, size_t pos)
{
	if (pos > buf->size) {
		return -EINVAL;
	}

	buf->len = pos;

	return 0;
}

int ctr_buf_append_mem(struct ctr_buf *buf, const uint8_t *mem, size_t len)
{
	if (ctr_buf_get_free(buf) < len) {
		return -ENOSPC;
	}

	memcpy(&buf->mem[buf->len], mem, len);
	buf->len += len;

	return 0;
}

int ctr_buf_append_str(struct ctr_buf *buf, const char *str)
{
	size_t len = strlen(str);

	if (ctr_buf_get_free(buf) < len + 1) {
		return -ENOSPC;
	}

	strcpy(&buf->mem[buf->len], str);
	buf->len += len + 1;

	return 0;
}

int ctr_buf_append_char(struct ctr_buf *buf, char val)
{
	if (ctr_buf_get_free(buf) < 1) {
		return -ENOSPC;
	}

	buf->mem[buf->len] = val;
	buf->len += 1;

	return 0;
}

int ctr_buf_append_s8(struct ctr_buf *buf, int8_t val)
{
	if (ctr_buf_get_free(buf) < 1) {
		return -ENOSPC;
	}

	buf->mem[buf->len] = val;
	buf->len += 1;

	return 0;
}

int ctr_buf_append_s16_le(struct ctr_buf *buf, int16_t val)
{
	if (ctr_buf_get_free(buf) < 2) {
		return -ENOSPC;
	}

	sys_put_le16(val, &buf->mem[buf->len]);
	buf->len += 2;

	return 0;
}

int ctr_buf_append_s16_be(struct ctr_buf *buf, int16_t val)
{
	if (ctr_buf_get_free(buf) < 2) {
		return -ENOSPC;
	}

	sys_put_be16(val, &buf->mem[buf->len]);
	buf->len += 2;

	return 0;
}

int ctr_buf_append_s32_le(struct ctr_buf *buf, int32_t val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

	sys_put_le32(val, &buf->mem[buf->len]);
	buf->len += 4;

	return 0;
}

int ctr_buf_append_s32_be(struct ctr_buf *buf, int32_t val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

	sys_put_be32(val, &buf->mem[buf->len]);
	buf->len += 4;

	return 0;
}

int ctr_buf_append_s64_le(struct ctr_buf *buf, int64_t val)
{
	if (ctr_buf_get_free(buf) < 8) {
		return -ENOSPC;
	}

	sys_put_le64(val, &buf->mem[buf->len]);
	buf->len += 8;

	return 0;
}

int ctr_buf_append_s64_be(struct ctr_buf *buf, int64_t val)
{
	if (ctr_buf_get_free(buf) < 8) {
		return -ENOSPC;
	}

	sys_put_be64(val, &buf->mem[buf->len]);
	buf->len += 8;

	return 0;
}

int ctr_buf_append_u8(struct ctr_buf *buf, uint8_t val)
{
	if (ctr_buf_get_free(buf) < 1) {
		return -ENOSPC;
	}

	buf->mem[buf->len] = val;
	buf->len += 1;

	return 0;
}

int ctr_buf_append_u16_le(struct ctr_buf *buf, uint16_t val)
{
	if (ctr_buf_get_free(buf) < 2) {
		return -ENOSPC;
	}

	sys_put_le16(val, &buf->mem[buf->len]);
	buf->len += 2;

	return 0;
}

int ctr_buf_append_u16_be(struct ctr_buf *buf, uint16_t val)
{
	if (ctr_buf_get_free(buf) < 2) {
		return -ENOSPC;
	}

	sys_put_be16(val, &buf->mem[buf->len]);
	buf->len += 2;

	return 0;
}

int ctr_buf_append_u32_le(struct ctr_buf *buf, uint32_t val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

	sys_put_le32(val, &buf->mem[buf->len]);
	buf->len += 4;

	return 0;
}

int ctr_buf_append_u32_be(struct ctr_buf *buf, uint32_t val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

	sys_put_be32(val, &buf->mem[buf->len]);
	buf->len += 4;

	return 0;
}

int ctr_buf_append_u64_le(struct ctr_buf *buf, uint64_t val)
{
	if (ctr_buf_get_free(buf) < 8) {
		return -ENOSPC;
	}

	sys_put_le64(val, &buf->mem[buf->len]);
	buf->len += 8;

	return 0;
}

int ctr_buf_append_u64_be(struct ctr_buf *buf, uint64_t val)
{
	if (ctr_buf_get_free(buf) < 8) {
		return -ENOSPC;
	}

	sys_put_be64(val, &buf->mem[buf->len]);
	buf->len += 8;

	return 0;
}

int ctr_buf_append_float_le(struct ctr_buf *buf, float val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	sys_mem_swap(&val, sizeof(val));
#endif

	memcpy(&buf->mem[buf->len], &val, 4);
	buf->len += 4;

	return 0;
}

int ctr_buf_append_float_be(struct ctr_buf *buf, float val)
{
	if (ctr_buf_get_free(buf) < 4) {
		return -ENOSPC;
	}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	sys_mem_swap(&val, sizeof(val));
#endif

	memcpy(&buf->mem[buf->len], &val, 4);
	buf->len += 4;

	return 0;
}
