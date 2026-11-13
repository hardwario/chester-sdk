/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_send_split.h"

/* Standard includes */
#include <errno.h>
#include <string.h>

bool app_send_select_empty(const struct app_send_select *sel)
{
	if (sel->ble_tags) {
		return false;
	}

	for (int i = 0; i < APP_SEND_SLOTS; i++) {
		if (!app_send_range_empty(&sel->rec[i]) || !app_send_range_empty(&sel->win[i])) {
			return false;
		}
	}

	return true;
}

/*
 * Move as much of the pending range into the message range as fits next to what the
 * message already holds: the whole range first, then the largest part that fits (binary
 * search). Returns -ENOSPC if not even one entry fits.
 */
static int fill(struct app_send_select *msg, struct app_send_range *msg_range,
		struct app_send_range *pending, const struct app_send_split_ops *ops, void *user)
{
	int ret;
	uint32_t count = pending->end - pending->first;
	uint32_t fits = 0;      /* largest count known to fit */
	uint32_t limit = count; /* largest count that may fit */

	msg_range->first = pending->first;

	while (fits < limit) {
		uint32_t n = fits == 0 && limit == count ? count : fits + (limit - fits + 1) / 2;

		msg_range->end = pending->first + n;

		ret = ops->encode(msg, user);
		if (!ret) {
			fits = n;
		} else if (ret == -ENOSPC) {
			limit = n - 1;
		} else {
			msg_range->end = msg_range->first;
			return ret;
		}
	}

	msg_range->end = pending->first + fits;
	pending->first += fits;

	return fits ? 0 : -ENOSPC;
}

static int send_ble_tags(struct app_send_select *msg, const struct app_send_split_ops *ops,
			 void *user)
{
	int ret;

	msg->ble_tags = true;

	ret = ops->encode(msg, user);
	if (ret != -ENOSPC) {
		return ret;
	}

	/* Send what the message holds and put the BLE tags into a message of their own */
	msg->ble_tags = false;

	if (!app_send_select_empty(msg)) {
		ret = ops->send(msg, user);
		if (ret) {
			return ret;
		}
	}

	memset(msg, 0, sizeof(*msg));
	msg->ble_tags = true;

	ret = ops->encode(msg, user);
	if (ret == -ENOSPC) {
		msg->ble_tags = false;
		ops->drop_ble_tags(user);
		return 0;
	}

	return ret;
}

int app_send_split(struct app_send_select *pending, const struct app_send_split_ops *ops,
		   void *user)
{
	int ret;
	struct app_send_select msg = {0};

	for (int i = 0; i < APP_SEND_SLOTS; i++) {
		for (int w = 0; w < 2; w++) {
			struct app_send_range *p = w ? &pending->win[i] : &pending->rec[i];
			struct app_send_range *m = w ? &msg.win[i] : &msg.rec[i];

			while (!app_send_range_empty(p)) {
				ret = fill(&msg, m, p, ops, user);
				if (!ret && app_send_range_empty(p)) {
					/* All of it fits, continue with the next range */
					break;
				}

				if (ret && ret != -ENOSPC) {
					return ret;
				}

				if (app_send_select_empty(&msg)) {
					/* Not even one entry fits an empty message */
					ops->drop(i, w, p->first, user);
					p->first++;
					continue;
				}

				/* The message is full */
				ret = ops->send(&msg, user);
				if (ret) {
					return ret;
				}

				memset(&msg, 0, sizeof(msg));
			}
		}
	}

	if (pending->ble_tags) {
		ret = send_ble_tags(&msg, ops, user);
		if (ret) {
			return ret;
		}

		pending->ble_tags = false;
	}

	if (!app_send_select_empty(&msg)) {
		return ops->send(&msg, user);
	}

	return 0;
}
