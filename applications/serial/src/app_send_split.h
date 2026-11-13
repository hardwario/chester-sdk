/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SEND_SPLIT_H_
#define APP_SEND_SPLIT_H_

/**
 * @file app_send_split.h
 * @brief Splitting of a report into messages that fit the payload buffer
 *
 * A report carries the records and windows of every device slot and the BLE tag
 * measurements. When it does not fit one message, app_send_split() packs it into as few
 * messages as it can: ranges are added in slot order and halved until they fit next to
 * what the message already holds. A single entry that does not fit an otherwise empty
 * message is dropped. The first failed send stops the report; what was not sent stays
 * for the next one.
 *
 * The module has no dependency on the encoder or the transport (callbacks), so it is
 * unit-tested on its own.
 */

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of device slots (APP_CONFIG_MAX_DEVICES) */
#define APP_SEND_SLOTS 8

/** Range of sequence numbers [first, end) in a ring of a device slot */
struct app_send_range {
	uint32_t first;
	uint32_t end;
};

/** What one message carries (every message lists all configured devices) */
struct app_send_select {
	uint32_t sequence;                         /**< Message sequence number */
	struct app_send_range rec[APP_SEND_SLOTS]; /**< Raw records per device */
	struct app_send_range win[APP_SEND_SLOTS]; /**< Aggregation windows per device */
	bool ble_tags;                             /**< BLE tag measurements */
};

struct app_send_split_ops {
	/** Encode a message; -ENOSPC if it does not fit the payload buffer */
	int (*encode)(struct app_send_select *sel, void *user);
	/** Encode and send a message, then drop what it carried */
	int (*send)(struct app_send_select *sel, void *user);
	/** Drop one entry that does not fit a message */
	void (*drop)(int slot, bool windows, uint32_t seq, void *user);
	/** Drop the BLE tag measurements (they do not fit a message) */
	void (*drop_ble_tags)(void *user);
};

static inline bool app_send_range_empty(const struct app_send_range *range)
{
	return range->first == range->end;
}

/** True if the message carries nothing besides the device list */
bool app_send_select_empty(const struct app_send_select *sel);

/**
 * @brief Send a report that does not fit one message in several messages
 *
 * @param pending Report content; consumed ranges are advanced
 * @return 0 when everything was sent or dropped, negative error code of the first failed
 *         encode or send otherwise
 */
int app_send_split(struct app_send_select *pending, const struct app_send_split_ops *ops,
		   void *user);

#ifdef __cplusplus
}
#endif

#endif /* APP_SEND_SPLIT_H_ */
