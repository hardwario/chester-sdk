#ifndef HIO_LTE_TALK_H
#define HIO_LTE_TALK_H

#include <hio_sys.h>
#include <stdarg.h>

int
hio_lte_talk_cmd(const char *fmt, ...);

int
hio_lte_talk_rsp(char **s, hio_sys_timeout_t timeout);

int
hio_lte_talk_ok(hio_sys_timeout_t timeout);

int
hio_lte_talk_cmd_ok(hio_sys_timeout_t timeout, const char *fmt, ...);

#endif
