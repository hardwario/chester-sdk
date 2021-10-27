#ifndef CHESTER_INCLUDE_LTE_TOK_H_
#define CHESTER_INCLUDE_LTE_TOK_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *hio_lte_tok_pfx(const char *s, const char *pfx);
char *hio_lte_tok_sep(const char *s);
char *hio_lte_tok_end(const char *s);
char *hio_lte_tok_str(const char *s, bool *def, char *str, size_t size);
char *hio_lte_tok_num(const char *s, bool *def, long *num);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LTE_TOK_H_ */
