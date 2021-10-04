#include <hio_buf.h>

// Standard includes
#include <errno.h>
#include <string.h>

int hio_buf_init(struct hio_buf *ctx, void *buf, size_t size)
{
    if (size == 0) {
        return -EINVAL;
    }

    ctx->mem = buf;
    ctx->size = size;

    hio_buf_reset(ctx);

    return 0;
}

uint8_t *hio_buf_get_mem(struct hio_buf *ctx)
{
    return ctx->mem;
}

size_t hio_buf_get_free(struct hio_buf *ctx)
{
    return ctx->size - ctx->len;
}

size_t hio_buf_get_used(struct hio_buf *ctx)
{
    return ctx->len;
}

void hio_buf_reset(struct hio_buf *ctx)
{
    ctx->len = 0;
}

void hio_buf_fill(struct hio_buf *ctx, int val)
{
    memset(ctx->mem, val, ctx->size);

    hio_buf_reset(ctx);
}

#define HIO_BUF_APPEND(_name, _type)                                          \
int hio_buf_append_ ## _name(struct hio_buf *ctx, _type val)                  \
{                                                                             \
    if (hio_buf_get_free(ctx) < sizeof(val)) {                                \
        return -ENOSPC;                                                       \
    }                                                                         \
                                                                              \
    for (size_t i = 0; i < sizeof(val); i++) {                                \
        ctx->mem[ctx->len++] = (uint64_t)val >> (8 * i);                      \
    }                                                                         \
                                                                              \
    return 0;                                                     \
}

HIO_BUF_APPEND(char, char)
HIO_BUF_APPEND(float, float)
HIO_BUF_APPEND(s8, int8_t)
HIO_BUF_APPEND(s16, int16_t)
HIO_BUF_APPEND(s32, int32_t)
HIO_BUF_APPEND(s64, int64_t)
HIO_BUF_APPEND(u8, uint8_t)
HIO_BUF_APPEND(u16, uint16_t)
HIO_BUF_APPEND(u32, uint32_t)
HIO_BUF_APPEND(u64, uint64_t)
