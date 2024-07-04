/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_LTE_V2_TOK_H_
#define CHESTER_SUBSYS_CTR_LTE_V2_TOK_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *ctr_lte_v2_tok_pfx(const char *s, const char *pfx);
char *ctr_lte_v2_tok_sep(const char *s);
char *ctr_lte_v2_tok_end(const char *s);
char *ctr_lte_v2_tok_str(const char *s, bool *def, char *str, size_t size);
char *ctr_lte_v2_tok_num(const char *s, bool *def, long *num);
char *ctr_lte_v2_tok_float(const char *s, bool *def, float *num);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_TOK_H_ */
