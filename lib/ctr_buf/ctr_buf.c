/* CHESTER includes */
#include <ctr_buf.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

int ctr_buf_init(struct ctr_buf *ctx, void *mem, size_t size)
{
	if (size == 0) {
		return -EINVAL;
	}

	ctx->mem = mem;
	ctx->size = size;

	ctr_buf_reset(ctx);

	return 0;
}

uint8_t *ctr_buf_get_mem(struct ctr_buf *ctx)
{
	return ctx->mem;
}

size_t ctr_buf_get_free(struct ctr_buf *ctx)
{
	return ctx->size - ctx->len;
}

size_t ctr_buf_get_used(struct ctr_buf *ctx)
{
	return ctx->len;
}

void ctr_buf_reset(struct ctr_buf *ctx)
{
	ctx->len = 0;
}

void ctr_buf_fill(struct ctr_buf *ctx, int val)
{
	memset(ctx->mem, val, ctx->size);

	ctr_buf_reset(ctx);
}

#define CTR_BUF_APPEND(_name, _type)                                                               \
	int ctr_buf_append_##_name(struct ctr_buf *ctx, _type val)                                 \
	{                                                                                          \
		if (ctr_buf_get_free(ctx) < sizeof(val)) {                                         \
			return -ENOSPC;                                                            \
		}                                                                                  \
                                                                                                   \
		for (size_t i = 0; i < sizeof(val); i++) {                                         \
			ctx->mem[ctx->len++] = (uint64_t)val >> (8 * i);                           \
		}                                                                                  \
                                                                                                   \
		return 0;                                                                          \
	}

CTR_BUF_APPEND(char, char)
CTR_BUF_APPEND(float, float)
CTR_BUF_APPEND(s8, int8_t)
CTR_BUF_APPEND(s16, int16_t)
CTR_BUF_APPEND(s32, int32_t)
CTR_BUF_APPEND(s64, int64_t)
CTR_BUF_APPEND(u8, uint8_t)
CTR_BUF_APPEND(u16, uint16_t)
CTR_BUF_APPEND(u32, uint32_t)
CTR_BUF_APPEND(u64, uint64_t)
