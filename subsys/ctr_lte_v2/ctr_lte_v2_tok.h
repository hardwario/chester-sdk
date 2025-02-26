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

/**
 * @brief Checks if a string starts with a given prefix.
 *
 * This function returns a pointer to the string if the prefix matches, or NULL otherwise.
 *
 * @param s     The input string to check.
 * @param pfx   The prefix to check for.
 * @return      Pointer to the start of the string if it matches the prefix, or NULL if it doesn't.
 */
const char *ctr_lte_v2_tok_pfx(const char *s, const char *pfx);

/**
 * @brief Checks if the character in the string is a separator.
 *
 * This function checks if the current character in the string matches a separator,
 * such as a comma, space, or other delimiter used in parsing.
 *
 * @param s     The input string.
 * @return      Pointer to the character if it is a separator, or NULL if it is not.
 */
const char *ctr_lte_v2_tok_sep(const char *s);

/**
 * @brief Returns a pointer to the end of a tokenized string.
 *
 * This function moves to the end of a token or substring within the given string.
 *
 * @param s     The input string.
 * @return      Pointer to the end of the current token within the string.
 */
const char *ctr_lte_v2_tok_end(const char *s);

/**
 * @brief Extracts a string token from the input and stores it in a buffer.
 *
 * This function parses a tokenized string and extracts a substring that is expected
 * to be enclosed in double quotes (`"`), storing it in the provided buffer.
 * Optionally indicates if a default value should be used.
 *
 * @param s     The input string.
 * @param def   A pointer to a boolean indicating if a default is used (output).
 * @param str   The buffer to store the extracted string.
 * @param size  The size of the buffer.
 * @return      Pointer to the next position in the string after the token.
 */
const char *ctr_lte_v2_tok_str(const char *s, bool *def, char *str, size_t size);

/**
 * @brief Extracts a numerical token from the input string.
 *
 * This function parses a tokenized string to extract a numerical value.
 * Optionally indicates if a default value should be used.
 *
 * @param s     The input string.
 * @param def   A pointer to a boolean indicating if a default is used (output).
 * @param num   A pointer to a long to store the extracted number.
 * @return      Pointer to the next position in the string after the token.
 */
const char *ctr_lte_v2_tok_num(const char *s, bool *def, long *num);

/**
 * @brief Extracts a floating-point token from the input string.
 *
 * This function parses a tokenized string to extract a floating-point value.
 * Optionally indicates if a default value should be used.
 *
 * @param s     The input string.
 * @param def   A pointer to a boolean indicating if a default is used (output).
 * @param num   A pointer to a float to store the extracted number.
 * @return      Pointer to the next position in the string after the token.
 */
const char *ctr_lte_v2_tok_float(const char *s, bool *def, float *num);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LTE_V2_TOK_H_ */
