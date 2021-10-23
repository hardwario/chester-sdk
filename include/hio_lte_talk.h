#ifndef CHESTER_INCLUDE_LTE_TALK_H_
#define CHESTER_INCLUDE_LTE_TALK_H_

/* Standard includes */
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int hio_lte_talk_cmd(const char *fmt, ...);
int hio_lte_talk_rsp(char **s, int64_t timeout);
int hio_lte_talk_ok(int64_t timeout);
int hio_lte_talk_cmd_ok(int64_t timeout, const char *fmt, ...);
void hio_lte_talk_cereg(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LTE_TALK_H_ */
