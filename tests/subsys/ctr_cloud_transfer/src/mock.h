/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef TESTS_SUBSYS_CTR_CLOUD_TRANSFER_SRC_MOCK_H_
#define TESTS_SUBSYS_CTR_CLOUD_TRANSFER_SRC_MOCK_H_

/* Standard includes */
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The call lasts its whole timeout */
#define MOCK_LTE_UNTIL_TIMEOUT INT_MAX

/* The timeout of a call was K_FOREVER */
#define MOCK_LTE_FOREVER (-1)

/* One ctr_lte_v2_send_recv() call: the packet the transfer is expected to send, how long the call
 * takes and what it returns */
struct mock_lte_exchange {
	uint16_t sequence;
	uint8_t flags;
	/* Sent without a response expected (the final ACK) */
	bool no_recv;
	int duration_ms;
	int ret;
	/* The response packet, if the call succeeds and expects one */
	uint16_t resp_sequence;
	uint8_t resp_flags;
	const char *resp_data;
};

/* When a call started and the timeout it got (ms, or MOCK_LTE_FOREVER) */
struct mock_lte_call {
	int64_t start;
	int64_t timeout_ms;
};

/* Load a script of exchanges; a call past its end, or one that sends another packet, fails the
 * test */
void mock_lte_start(uint32_t serial_number, const uint8_t token[16],
		    const struct mock_lte_exchange *exchanges, size_t count);

/* The call (one of those made so far) */
const struct mock_lte_call *mock_lte_call(size_t index);

/* Assert that the script ran to its end */
void mock_lte_assert_done(void);

#endif
