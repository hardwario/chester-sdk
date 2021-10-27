#ifndef CHESTER_INCLUDE_BUF_H_
#define CHESTER_INCLUDE_BUF_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIO_BUF_DEFINE(_name, _size)                                                               \
	uint8_t _name##_mem[_size];                                                                \
	struct hio_buf _name = {                                                                   \
		.mem = _name##_mem,                                                                \
		.size = sizeof(_name##_mem),                                                       \
		.len = 0,                                                                          \
	}

#define HIO_BUF_DEFINE_STATIC(_name, _size)                                                        \
	static uint8_t _name##_mem[_size];                                                         \
	static struct hio_buf _name = {                                                            \
		.mem = _name##_mem,                                                                \
		.size = sizeof(_name##_mem),                                                       \
		.len = 0,                                                                          \
	}

struct hio_buf {
	uint8_t *mem;
	size_t size;
	size_t len;
};

int hio_buf_init(struct hio_buf *ctx, void *mem, size_t size);
uint8_t *hio_buf_get_mem(struct hio_buf *ctx);
size_t hio_buf_get_free(struct hio_buf *ctx);
size_t hio_buf_get_used(struct hio_buf *ctx);
void hio_buf_reset(struct hio_buf *ctx);
void hio_buf_fill(struct hio_buf *ctx, int val);
int hio_buf_append_char(struct hio_buf *ctx, char val);
int hio_buf_append_float(struct hio_buf *ctx, float val);
int hio_buf_append_s8(struct hio_buf *ctx, int8_t val);
int hio_buf_append_s16(struct hio_buf *ctx, int16_t val);
int hio_buf_append_s32(struct hio_buf *ctx, int32_t val);
int hio_buf_append_s64(struct hio_buf *ctx, int64_t val);
int hio_buf_append_u8(struct hio_buf *ctx, uint8_t val);
int hio_buf_append_u16(struct hio_buf *ctx, uint16_t val);
int hio_buf_append_u32(struct hio_buf *ctx, uint32_t val);
int hio_buf_append_u64(struct hio_buf *ctx, uint64_t val);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BUF_H_ */
