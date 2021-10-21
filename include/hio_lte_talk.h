#ifndef HIO_LTE_TALK_H
#define HIO_LTE_TALK_H

// Standard includes
#include <stdarg.h>
#include <stdint.h>

int hio_lte_talk_cmd(const char *fmt, ...);
int hio_lte_talk_rsp(char **s, int64_t timeout);
int hio_lte_talk_ok(int64_t timeout);
int hio_lte_talk_cmd_ok(int64_t timeout, const char *fmt, ...);
void hio_lte_talk_cereg(const char *s);

#endif
