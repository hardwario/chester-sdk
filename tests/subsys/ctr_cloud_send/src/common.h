/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef TESTS_SUBSYS_CTR_CLOUD_SEND_SRC_COMMON_H_
#define TESTS_SUBSYS_CTR_CLOUD_SEND_SRC_COMMON_H_

#include "ctr_cloud_spool.h"

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEST_BUF_SIZE CONFIG_CTR_CLOUD_TRANSFER_BUF_SIZE
#define TEST_HDR_LEN  9

/* {1: 42} exactly as zcbor encodes it (indefinite-length map) */
extern const uint8_t PAYLOAD_A[5];
extern const uint8_t MSG_A[TEST_HDR_LEN + 5];
/* [7, 8]; differs from PAYLOAD_A already in the first byte */
extern const uint8_t PAYLOAD_B[3];
extern const uint8_t MSG_B[TEST_HDR_LEN + 3];

/* ---- Mock transport (ctr_cloud_transfer_*) ---- */

#define MOCK_UPLINK_MAX     8
#define MOCK_UPLINK_MEM_MAX 32

struct mock_uplink {
	size_t len;
	uint8_t mem[MOCK_UPLINK_MEM_MAX];
	k_timeout_t timeout;
	int ret;
};

void mock_transfer_reset(void);
void mock_transfer_set_init_ret(int ret);
/* Next uplink is recorded and then fails with `err` */
void mock_transfer_fail_next_uplink(int err);
/* Next uplink gives `entered` and waits for mock_transfer_release() */
void mock_transfer_gate_next_uplink(void);
int mock_transfer_wait_entered(k_timeout_t timeout);
void mock_transfer_release(void);
/* Release the gated uplink from a timer after `delay` */
void mock_transfer_release_after(k_timeout_t delay);
int mock_transfer_uplink_count(void);
const struct mock_uplink *mock_transfer_uplink(int index);
int mock_transfer_downlink_count(void);

/* Every uplink since the start, also the session request of the init work; unlike the
 * above, not cleared by mock_transfer_reset()
 */
#define MOCK_LOG_MAX     32
#define MOCK_LOG_MEM_MAX 64

struct mock_log_entry {
	size_t len;
	uint8_t mem[MOCK_LOG_MEM_MAX];
	int ret;
	/* Uptime of the call in ms */
	int64_t at;
};

int mock_transfer_log_count(void);
const struct mock_log_entry *mock_transfer_log(int index);
/* Index of the first logged uplink of message `type`, or -1 */
int mock_transfer_log_find(uint8_t type);
/* The first `count` uplinks of message `type` fail with `err`; not cleared by
 * mock_transfer_reset()
 */
void mock_transfer_fail_first_of_type(uint8_t type, int err, int count);
/* The first session response comes without an id; not cleared by mock_transfer_reset() */
void mock_transfer_session_without_id_first(void);

/* ---- Mock MCUboot image state and the firmware update scenario (TEST_FW) ---- */

/* TEST_FW 0: no update id stored, 1: id stored and acked, 2: id stored, the first ack fails,
 * 3: id stored, but the image is confirmed (MCUboot reverted the update), 4: id stored, but the
 * image cannot be confirmed
 */
extern const uint8_t TEST_FW_UPDATE_ID[16];

/* Before ctr_cloud_init(): an unconfirmed image and the update id of TEST_FW */
void mock_fw_setup(void);
int mock_boot_confirm_calls(void);

/* ---- Mock spool backend (ctr_cloud_spool_*) ---- */

void mock_spool_reset(void);
void mock_spool_fail_next_save(int err);
void mock_spool_fail_next_delete(int err);
int mock_spool_save_count(void);
int mock_spool_delete_count(void);
int mock_spool_load_count(void);
/* Bytes of the most recent successful save */
const uint8_t *mock_spool_last_saved(size_t *len);
/* Number of stored messages */
int mock_spool_stored(void);

/* ---- Helpers ---- */

/* For the init work, which pauses 10 s between the failed attempts of a step (TEST_INIT_RETRY,
 * TEST_FW 2)
 */
#define TEST_INIT_TIMEOUT K_SECONDS(60)

/* Call ctr_cloud_init() once with the test options; later calls return 0 */
int common_init_start(void);
/* common_init_start() and wait for the session (over the mock transport) */
void *common_setup_initialized(void);
/* Reset the mocks */
void common_before(void *fixture);

uint32_t uplink_data_count(void);

/* Assert that uplink `index` carried exactly `msg` */
void assert_uplink(int index, const uint8_t *msg, size_t len);
/* Assert that the uplink got a timeout in (min_ms, max_ms] */
void assert_timeout_left(const struct mock_uplink *u, uint32_t min_ms, uint32_t max_ms);
/* Assert that nothing was sent, saved or downlinked since common_before() */
void assert_no_transfer(void);

/* Run `fn(arg)` in the helper thread (one at a time) */
void helper_start(int (*fn)(void *arg), void *arg);
/* Join the helper thread and return the value of its function */
int helper_join(void);
/* After each test: clean up a helper thread a failed test left behind */
void common_after(void *fixture);

/* Assert that the message went through the spool (when enabled) as exactly one save of
 * `msg`, with `deleted` deletes; without the spool, that it was never saved */
void assert_spooled_once(const uint8_t *msg, size_t len, int deleted);
/* Assert one uplink of `msg` with K_FOREVER, no downlink, uplink_data_count() == base + 1 */
void assert_one_data_uplink(const uint8_t *msg, size_t len, uint32_t base);

/* ctr_cloud_send_data(PAYLOAD_A, K_FOREVER): the "other sender" of the helper thread */
int send_a_forever(void *arg);

/* A sender under test: ctr_cloud_send_data*() of PAYLOAD_A / PAYLOAD_B, K_FOREVER */
typedef int (*send_fn)(void);

/* Another sender holds the lock for 300 ms of the 1000 ms deadline of `send_b`; assert
 * that both messages are sent and the transfer of `send_b` gets what is left (600-700 ms) */
void assert_deadline_after_lock_wait(int (*send_b)(void));
/* The first send fails (-EIO) and stays spooled (when enabled); the next one sends the
 * newest first and then drains the spool */
void run_error_kept_then_drained(send_fn send_a, send_fn send_b);

/* Assert that another thread can take the cloud lock with K_NO_WAIT (counts a downlink) */
void assert_lock_free(void);

#endif /* TESTS_SUBSYS_CTR_CLOUD_SEND_SRC_COMMON_H_ */
