/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Mock retained_mem device backed by a plain static array. Two instances are
 * provided:
 *   - the DT `ctr,trace_mem` chosen device, used by the ctr_trace subsystem's
 *     own SYS_INIT (backing store hidden), and
 *   - a standalone scratch device (`mock_test_dev()` / `mock_test_backing`) that
 *     the ring/reason unit tests drive directly, so the subsystem's live log
 *     backend never races the test data.
 */

#ifndef MOCK_RETMEM_H_
#define MOCK_RETMEM_H_

#include <zephyr/device.h>

#include <stddef.h>
#include <stdint.h>

#define MOCK_RETMEM_SIZE 8192

/** Backing store of the standalone scratch device (white-box access). */
extern uint8_t mock_test_backing[MOCK_RETMEM_SIZE];

/** Standalone scratch retained_mem device for unit tests. */
const struct device *mock_test_dev(void);

/** Zero the scratch backing store. */
void mock_test_reset(void);

#endif /* MOCK_RETMEM_H_ */
