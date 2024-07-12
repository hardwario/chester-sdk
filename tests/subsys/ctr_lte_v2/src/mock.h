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

void mock_ctr_lte_link_start(struct mock_link_item *items, size_t count);

#endif
