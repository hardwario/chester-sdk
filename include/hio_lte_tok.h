#ifndef HIO_LTE_PARSE_H
#define HIO_LTE_PARSE_H

// Standard includes
#include <stdbool.h>
#include <stddef.h>

char *
hio_lte_tok_pfx(const char *s, const char *pfx);

char *
hio_lte_tok_sep(const char *s);

char *
hio_lte_tok_end(const char *s);

char *
hio_lte_tok_str(const char *s, bool *def, char *str, size_t size);

char *
hio_lte_tok_num(const char *s, bool *def, long *num);

#endif
