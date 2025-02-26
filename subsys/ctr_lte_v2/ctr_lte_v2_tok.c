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

const char *ctr_lte_v2_tok_pfx(const char *s, const char *pfx)
{
	if (!s || !pfx) {
		return NULL;
	}

	size_t len = strlen(pfx);

	if (!len) {
		return NULL;
	}

	if (strncmp(s, pfx, len) != 0) {
		return NULL;
	}

	return s + len;
}

const char *ctr_lte_v2_tok_sep(const char *s)
{
	if (!s || *s != ',') {
		return NULL;
	}

	return s + 1;
}

const char *ctr_lte_v2_tok_end(const char *s)
{
	if (!s || *s != '\0') {
		return NULL;
	}
	return s;
}

const char *ctr_lte_v2_tok_str(const char *s, bool *def, char *str, size_t size)
{
	if (!s) {
		return NULL;
	}

	if (def) {
		*def = false;
	}

	if (str && size > 0) {
		str[0] = '\0';
	}

	if (*s == '\0' || *s == ',') {
		return (char *)s;
	}

	if (*s != '"') {
		return NULL;
	}

	const char *end_quote = strchr(s + 1, '"');

	if (!end_quote) {
		return NULL;
	}

	if (def) {
		*def = true;
	}

	if (str) {
		size_t len = end_quote - s - 1;

		if (len >= size) {
			return NULL;
		}

		strncpy(str, s + 1, len);
		str[len] = '\0';
	}

	return end_quote + 1;
}

const char *ctr_lte_v2_tok_num(const char *s, bool *def, long *num)
{
	if (!s) {
		return NULL;
	}

	if (def) {
		*def = false;
	}

	if (num) {
		*num = 0;
	}

	if (*s == '\0' || *s == ',') {
		return (char *)s;
	}

	if (isdigit((int)*s) == 0 && *s != '-') {
		return NULL;
	}

	char *end;
	long value = strtol(s, &end, 10);

	if (*end != '\0' && *end != ',') {
		return NULL;
	}

	if (def) {
		*def = true;
	}

	if (num) {
		*num = value;
	}

	return end;
}

const char *ctr_lte_v2_tok_float(const char *s, bool *def, float *num)
{
	if (!s) {
		return NULL;
	}

	if (def) {
		*def = false;
	}

	if (num) {
		*num = 0;
	}
	if (*s == '\0' || *s == ',') {
		return (char *)s;
	}

	char *end;
	float value = strtof(s, &end);

	if (end == s || (*end != '\0' && *end != ',')) {
		return NULL;
	}

	if (def) {
		*def = true;
	}

	if (num) {
		*num = value;
	}

	return end;
}
