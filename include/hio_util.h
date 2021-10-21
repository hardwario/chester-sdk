#ifndef HIO_UTIL_H
#define HIO_UTIL_H

// Standard includes
#include <stdbool.h>
#include <stddef.h>

int hio_buf2hex(const void *src, size_t src_size, char *dst, size_t dst_size, bool upper);
int hio_hex2buf(const char *src, void *dst, size_t dst_size, bool allow_spaces);

#endif
