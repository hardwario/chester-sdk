#ifndef HIO_LRW_UART_H
#define HIO_LRW_UART_H

// Standard includes
#include <stdarg.h>
#include <stdint.h>

typedef void (*hio_lrw_recv_cb)(const char *s);

int hio_lrw_uart_init(hio_lrw_recv_cb recv_cb);
int hio_lrw_uart_enable(void);
int hio_lrw_uart_disable(void);
int hio_lrw_uart_send(const char *fmt, va_list ap);

#endif
