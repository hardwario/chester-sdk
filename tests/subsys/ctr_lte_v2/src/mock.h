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

/* The longest script the mock keeps the times of */
#define MOCK_LINK_MAX_ITEMS 512

/* The resets of a script the mock keeps the places of */
#define MOCK_LINK_MAX_RESETS 8

struct mock_link_item {
	const char *tx;
	const char *rx;
	const char *rx2;
	int delay_ms;
};

typedef char **list;

/* Load a script of AT lines: an item with tx expects that line from the FSM and answers with rx
 * and rx2, an item without tx is a URC sent delay_ms after the previous item (or after the
 * start). A line the script does not expect at that point (also after its end) is a violation. */
void mock_ctr_lte_link_start(struct mock_link_item *items, size_t count);

/* When the item of the loaded script was handled, in k_uptime_get() ms: the FSM sent its line (tx)
 * or the mock sent the URC. -1 for an item not reached yet and for an index past the script. */
int64_t mock_ctr_lte_link_tx_time(size_t index);

/* Assert, from the test thread, that the script ran to its end without a violation */
void mock_ctr_lte_link_assert_done(void);

/* The resets of the modem so far (the reset is not a line of the script) */
int mock_ctr_lte_link_reset_count(void);

/* The next count wake-ups get no "Ready": the modem does not answer after a reset */
void mock_ctr_lte_link_drop_ready(int count);

/* Where the n-th reset (from 0) since mock_ctr_lte_link_start() came: the index of the script item
 * after it. -1 for a reset not made, or not followed by a wake-up before that item (the modem boots
 * on the wake-up after its reset) */
int mock_ctr_lte_link_reset_item(int n);

/* Model the subscriptions of +CEREG and +CSCON as mfw 1.3.6 with SLM 2.5.2 keeps them (off by
 * default): AT+CEREG=5 and AT+CSCON=1 make them, AT+CFUN=0 and a reset drop them (a line answered
 * other than OK changes nothing). A URC of the script without its subscription is not sent, the
 * script goes on past it. Turned on, the modem has none */
void mock_ctr_lte_link_model_subscriptions(bool on);

#endif
