#ifndef HIO_LTE_UART_H
#define HIO_LTE_UART_H

// Standard includes
#include <stdarg.h>
#include <stdint.h>

int
hio_lte_uart_init(void);

int
hio_lte_uart_send(const char *fmt, va_list ap);

int
hio_lte_uart_recv(char **s, int64_t timeout);

#endif
