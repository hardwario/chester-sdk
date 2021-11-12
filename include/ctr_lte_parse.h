#ifndef CHESTER_INCLUDE_LTE_PARSE_H_
#define CHESTER_INCLUDE_LTE_PARSE_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_lte_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
                       int *seconds);
int ctr_lte_parse_cereg(const char *s, int *stat);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LTE_PARSE_H_ */
