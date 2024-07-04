/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_tok.h"

/* Standard includes */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *ctr_lte_v2_tok_pfx(const char *s, const char *pfx)
{
	char *p = (char *)s;

	size_t len = strlen(pfx);

	if (strncmp(p, pfx, len) != 0) {
		return NULL;
	}

	return p + len;
}

char *ctr_lte_v2_tok_sep(const char *s)
{
	char *p = (char *)s;

	if (*p++ != ',') {
		return NULL;
	}

	return p;
}

char *ctr_lte_v2_tok_end(const char *s)
{
	char *p = (char *)s;

	if (*p != '\0') {
		return NULL;
	}

	return p;
}

char *ctr_lte_v2_tok_str(const char *s, bool *def, char *str, size_t size)
{
	if (str != NULL) {
		memset(str, 0, size);
	}

	if (def != NULL) {
		*def = false;
	}

	char *p = (char *)s;

	if (*p == '\0' || *p == ',') {
		return p;
	}

	if (*p++ != '"') {
		return NULL;
	}

	p = strchr(p, '"');

	if (p == NULL) {
		return NULL;
	}

	if (def != NULL) {
		*def = true;
	}

	if (str != NULL) {
		size_t len = p - s - 1;

		if (len >= size) {
			return NULL;
		}

		strncpy(str, s + 1, len);
	}

	return p + 1;
}

char *ctr_lte_v2_tok_num(const char *s, bool *def, long *num)
{
	if (num != NULL) {
		*num = 0;
	}

	if (def != NULL) {
		*def = false;
	}

	char *p = (char *)s;

	if (*p == '\0' || *p == ',') {
		return p;
	}

	if (isdigit((int)*p) == 0 && *p != '-') {
		return NULL;
	}

	p++;

	while (*p != '\0' && *p != ',') {
		if (isdigit((int)*p++) == 0) {
			return NULL;
		}
	}

	if (def != NULL) {
		*def = true;
	}

	if (*s == '-') {
		char buf[11 + 1] = {0};

		size_t len = p - s;

		if (len >= sizeof(buf)) {
			return NULL;
		}

		strncpy(buf, s, len);

		if (num != NULL) {
			*num = atol(buf);
		}

	} else {
		char buf[10 + 1] = {0};

		size_t len = p - s;

		if (len >= sizeof(buf)) {
			return NULL;
		}

		strncpy(buf, s, len);

		if (num != NULL) {
			*num = atol(buf);
		}
	}

	return p;
}

char *ctr_lte_v2_tok_float(const char *s, bool *def, float *num)
{
	if (!num) {
		return NULL;
	}

	*num = 0;
	*def = false;

	char *p = (char *)s;

	if (*p == '\0' || *p == ',') {
		return p;
	}

	if (isdigit((int)*p) == 0 && *p != '-') {
		return NULL;
	}

	p++;

	while (*p != '\0' && *p != ',') {
		if (isdigit((int)*p) == 0 && *p != '.') {
			return NULL;
		}

		p++;
	}

	if (def != NULL) {
		*def = true;
	}

	char buf[11 + 1] = {0};

	size_t len = p - s;

	if (len >= sizeof(buf)) {
		return NULL;
	}

	strncpy(buf, s, len);

	*num = strtof(s, &p);

	return p;
}
