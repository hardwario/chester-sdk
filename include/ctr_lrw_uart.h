#ifndef CHESTER_INCLUDE_LRW_UART_H_
#define CHESTER_INCLUDE_LRW_UART_H_

/* Standard includes */
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ctr_lrw_recv_cb)(const char *s);

int ctr_lrw_uart_init(ctr_lrw_recv_cb recv_cb);
int ctr_lrw_uart_enable(void);
int ctr_lrw_uart_disable(void);
int ctr_lrw_uart_send(const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LRW_UART_H_ */