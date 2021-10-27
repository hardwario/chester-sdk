#ifndef CHESTER_INCLUDE_BUF_H_
#define CHESTER_INCLUDE_BUF_H_

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

int ctr_buf_init(struct ctr_buf *ctx, void *mem, size_t size);
uint8_t *ctr_buf_get_mem(struct ctr_buf *ctx);
size_t ctr_buf_get_free(struct ctr_buf *ctx);
size_t ctr_buf_get_used(struct ctr_buf *ctx);
void ctr_buf_reset(struct ctr_buf *ctx);
void ctr_buf_fill(struct ctr_buf *ctx, int val);
int ctr_buf_append_char(struct ctr_buf *ctx, char val);
int ctr_buf_append_float(struct ctr_buf *ctx, float val);
int ctr_buf_append_s8(struct ctr_buf *ctx, int8_t val);
int ctr_buf_append_s16(struct ctr_buf *ctx, int16_t val);
int ctr_buf_append_s32(struct ctr_buf *ctx, int32_t val);
int ctr_buf_append_s64(struct ctr_buf *ctx, int64_t val);
int ctr_buf_append_u8(struct ctr_buf *ctx, uint8_t val);
int ctr_buf_append_u16(struct ctr_buf *ctx, uint16_t val);
int ctr_buf_append_u32(struct ctr_buf *ctx, uint32_t val);
int ctr_buf_append_u64(struct ctr_buf *ctx, uint64_t val);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BUF_H_ */
