/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_BUF_H_
#define CHESTER_INCLUDE_CTR_BUF_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_BUF_DEFINE(_name, _size)                                                               \
	uint8_t _name##_mem[_size];                                                                \
	struct ctr_buf _name = {                                                                   \
		.mem = _name##_mem,                                                                \
		.size = sizeof(_name##_mem),                                                       \
		.len = 0,                                                                          \
	}

#define CTR_BUF_DEFINE_STATIC(_name, _size)                                                        \
	static uint8_t _name##_mem[_size];                                                         \
	static struct ctr_buf _name = {                                                            \
		.mem = _name##_mem,                                                                \
		.size = sizeof(_name##_mem),                                                       \
		.len = 0,                                                                          \
	}

struct ctr_buf {
	uint8_t *mem;
	size_t size;
	size_t len;
};

int ctr_buf_init(struct ctr_buf *buf, void *mem, size_t size);
uint8_t *ctr_buf_get_mem(struct ctr_buf *buf);
size_t ctr_buf_get_free(struct ctr_buf *buf);
size_t ctr_buf_get_used(struct ctr_buf *buf);
void ctr_buf_reset(struct ctr_buf *buf);
void ctr_buf_fill(struct ctr_buf *buf, int val);
int ctr_buf_seek(struct ctr_buf *buf, size_t pos);
int ctr_buf_append_mem(struct ctr_buf *buf, const uint8_t *mem, size_t len);
int ctr_buf_append_str(struct ctr_buf *buf, const char *str);
int ctr_buf_append_char(struct ctr_buf *buf, char val);
int ctr_buf_append_s8(struct ctr_buf *buf, int8_t val);
int ctr_buf_append_s16_le(struct ctr_buf *buf, int16_t val);
int ctr_buf_append_s16_be(struct ctr_buf *buf, int16_t val);
int ctr_buf_append_s32_le(struct ctr_buf *buf, int32_t val);
int ctr_buf_append_s32_be(struct ctr_buf *buf, int32_t val);
int ctr_buf_append_s64_le(struct ctr_buf *buf, int64_t val);
int ctr_buf_append_s64_be(struct ctr_buf *buf, int64_t val);
int ctr_buf_append_u8(struct ctr_buf *buf, uint8_t val);
int ctr_buf_append_u16_le(struct ctr_buf *buf, uint16_t val);
int ctr_buf_append_u16_be(struct ctr_buf *buf, uint16_t val);
int ctr_buf_append_u32_le(struct ctr_buf *buf, uint32_t val);
int ctr_buf_append_u32_be(struct ctr_buf *buf, uint32_t val);
int ctr_buf_append_u64_le(struct ctr_buf *buf, uint64_t val);
int ctr_buf_append_u64_be(struct ctr_buf *buf, uint64_t val);
int ctr_buf_append_float_le(struct ctr_buf *buf, float val);
int ctr_buf_append_float_be(struct ctr_buf *buf, float val);

#define ctr_buf_append_s16(buf, val)   ctr_buf_append_s16_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_s32(buf, val)   ctr_buf_append_s32_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_s64(buf, val)   ctr_buf_append_s64_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_u16(buf, val)   ctr_buf_append_u16_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_u32(buf, val)   ctr_buf_append_u32_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_u64(buf, val)   ctr_buf_append_u64_le(buf, val) __DEPRECATED_MACRO
#define ctr_buf_append_float(buf, val) ctr_buf_append_float_le(buf, val) __DEPRECATED_MACRO

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_BUF_H_ */
