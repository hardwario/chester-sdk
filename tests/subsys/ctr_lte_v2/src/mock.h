#ifndef TESTS_SUBSYS_CTR_LTE_V2_SRC_MOCK_H_
#define TESTS_SUBSYS_CTR_LTE_V2_SRC_MOCK_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct mock_link_item {
	const char *tx;
	const char *rx;
	const char *rx2;
	int delay_ms;
};

typedef char **list;

/* Load a script of AT lines: an item with tx expects that line from the FSM and answers with rx
 * and rx2, an item without tx is a URC sent delay_ms after the previous item. A line the script
 * does not expect at that point (also after its end) is a violation. */
void mock_ctr_lte_link_start(struct mock_link_item *items, size_t count);

/* Assert, from the test thread, that the script ran to its end without a violation */
void mock_ctr_lte_link_assert_done(void);

#endif
