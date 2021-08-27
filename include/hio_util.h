#ifndef HIO_UTIL_H
#define HIO_UTIL_H

// Zephyr includes
#include <sys/util.h>
#include <zephyr.h>

// Standard includes
#include <stdbool.h>
#include <stddef.h>

#define HIO_ARRAY_SIZE ARRAY_SIZE
#define HIO_ARG_UNUSED ARG_UNUSED

int hio_buf2hex(const void *src, size_t src_size,
                char *dst, size_t dst_size, bool upper);

int hio_hex2buf(const char *src,
                void *dst, size_t dst_size, bool allow_spaces);

#endif
