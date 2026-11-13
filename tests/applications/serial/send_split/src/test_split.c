/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for splitting a report into messages, with a fake encoder that charges a fixed
 * size per message, record, window and the BLE tag block.
 */

#include "app_send_split.h"

#include <zephyr/ztest.h>

#include <errno.h>
#include <string.h>

#define MAX_MESSAGES 64
#define MAX_DROPS    64

struct fake {
	/* Encoder model */
	int capacity;
	int header;
	int rec_cost[APP_SEND_SLOTS];
	int win_cost;
	int ble_cost;
	int encode_error; /* returned by encode when non-zero */

	/* Transport model */
	int fail_send_at; /* index of the send that fails, -1 = none */

	/* Observed */
	int encodes;
	struct app_send_select sent[MAX_MESSAGES];
	int sent_count;
	struct {
		int slot;
		bool windows;
		uint32_t seq;
	} drops[MAX_DROPS];
	int drop_count;
	int ble_drops;
};

static struct fake m_fake;

static int cost(const struct fake *f, const struct app_send_select *sel)
{
	int size = f->header;

	for (int i = 0; i < APP_SEND_SLOTS; i++) {
		size += (int)(sel->rec[i].end - sel->rec[i].first) * f->rec_cost[i];
		size += (int)(sel->win[i].end - sel->win[i].first) * f->win_cost;
	}

	return size + (sel->ble_tags ? f->ble_cost : 0);
}

static int fake_encode(struct app_send_select *sel, void *user)
{
	struct fake *f = user;

	f->encodes++;

	if (f->encode_error) {
		return f->encode_error;
	}

	return cost(f, sel) > f->capacity ? -ENOSPC : 0;
}

static int fake_send(struct app_send_select *sel, void *user)
{
	struct fake *f = user;
	int ret = fake_encode(sel, user);

	if (ret) {
		return ret;
	}

	if (f->sent_count == f->fail_send_at) {
		return -EIO;
	}

	zassert_true(f->sent_count < MAX_MESSAGES);
	f->sent[f->sent_count++] = *sel;

	return 0;
}

static void fake_drop(int slot, bool windows, uint32_t seq, void *user)
{
	struct fake *f = user;

	zassert_true(f->drop_count < MAX_DROPS);
	f->drops[f->drop_count].slot = slot;
	f->drops[f->drop_count].windows = windows;
	f->drops[f->drop_count].seq = seq;
	f->drop_count++;
}

static void fake_drop_ble_tags(void *user)
{
	struct fake *f = user;

	f->ble_drops++;
}

static const struct app_send_split_ops m_ops = {
	.encode = fake_encode,
	.send = fake_send,
	.drop = fake_drop,
	.drop_ble_tags = fake_drop_ble_tags,
};

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	memset(&m_fake, 0, sizeof(m_fake));
	m_fake.capacity = 1000;
	m_fake.header = 100;
	m_fake.win_cost = 20;
	m_fake.ble_cost = 300;
	m_fake.fail_send_at = -1;

	for (int i = 0; i < APP_SEND_SLOTS; i++) {
		m_fake.rec_cost[i] = 30;
	}
}

/* The sent ranges of a ring cover [first, end) exactly once and in order */
static void check_coverage(int slot, bool windows, uint32_t first, uint32_t end)
{
	uint32_t next = first;

	for (int m = 0; m < m_fake.sent_count; m++) {
		const struct app_send_range *r =
			windows ? &m_fake.sent[m].win[slot] : &m_fake.sent[m].rec[slot];

		if (app_send_range_empty(r)) {
			continue;
		}

		zassert_equal(r->first, next, "slot %d message %d starts at %u, expected %u", slot,
			      m, r->first, next);
		next = r->end;
	}

	zassert_equal(next, end, "slot %d covered up to %u, expected %u", slot, next, end);
}

static void check_messages_fit(void)
{
	for (int m = 0; m < m_fake.sent_count; m++) {
		zassert_true(cost(&m_fake, &m_fake.sent[m]) <= m_fake.capacity, "message %d", m);
		zassert_false(app_send_select_empty(&m_fake.sent[m]), "message %d is empty", m);
	}
}

static int ble_messages(void)
{
	int n = 0;

	for (int m = 0; m < m_fake.sent_count; m++) {
		n += m_fake.sent[m].ble_tags;
	}

	return n;
}

ZTEST(send_split, test_everything_sent_once_in_order)
{
	struct app_send_select pending = {
		.rec[0] = {.first = 5, .end = 105},
		.rec[2] = {.first = 0, .end = 3},
		.win[3] = {.first = 10, .end = 30},
		.ble_tags = true,
	};

	zassert_ok(app_send_split(&pending, &m_ops, &m_fake));

	check_messages_fit();
	check_coverage(0, false, 5, 105);
	check_coverage(2, false, 0, 3);
	check_coverage(3, true, 10, 30);
	zassert_equal(ble_messages(), 1);
	zassert_equal(m_fake.drop_count, 0);
	zassert_equal(m_fake.ble_drops, 0);

	/* 100 + 30 * 30 = 1000: three messages of 30 records, then the last 10 records with
	 * slot 2 and the windows (890 B), and the BLE tags (300 B) in a message of their own
	 */
	zassert_equal(m_fake.sent_count, 5, "%d messages", m_fake.sent_count);
	zassert_equal(m_fake.sent[0].rec[0].end - m_fake.sent[0].rec[0].first, 30);
	zassert_true(m_fake.sent[4].ble_tags);
}

ZTEST(send_split, test_oversized_entries_dropped)
{
	/* A record of slot 1 does not fit even an empty message */
	m_fake.rec_cost[1] = 950;

	struct app_send_select pending = {
		.rec[0] = {.first = 0, .end = 4},
		.rec[1] = {.first = 7, .end = 10},
	};

	zassert_ok(app_send_split(&pending, &m_ops, &m_fake));

	check_messages_fit();
	check_coverage(0, false, 0, 4);
	zassert_equal(m_fake.drop_count, 3);

	for (int i = 0; i < 3; i++) {
		zassert_equal(m_fake.drops[i].slot, 1);
		zassert_false(m_fake.drops[i].windows);
		zassert_equal(m_fake.drops[i].seq, 7 + i);
	}
}

ZTEST(send_split, test_ble_tags_in_own_message)
{
	/* BLE tags fit only into an otherwise empty message */
	m_fake.ble_cost = 850;

	struct app_send_select pending = {
		.rec[0] = {.first = 0, .end = 10},
		.ble_tags = true,
	};

	zassert_ok(app_send_split(&pending, &m_ops, &m_fake));

	check_messages_fit();
	check_coverage(0, false, 0, 10);
	zassert_equal(ble_messages(), 1);
	zassert_true(m_fake.sent[m_fake.sent_count - 1].ble_tags);
	zassert_true(app_send_range_empty(&m_fake.sent[m_fake.sent_count - 1].rec[0]));
}

ZTEST(send_split, test_ble_tags_too_large)
{
	m_fake.ble_cost = 2000;

	struct app_send_select pending = {
		.rec[0] = {.first = 0, .end = 2},
		.ble_tags = true,
	};

	zassert_ok(app_send_split(&pending, &m_ops, &m_fake));

	check_coverage(0, false, 0, 2);
	zassert_equal(ble_messages(), 0);
	zassert_equal(m_fake.ble_drops, 1);
}

ZTEST(send_split, test_send_failure_stops)
{
	m_fake.fail_send_at = 1;

	struct app_send_select pending = {
		.rec[0] = {.first = 0, .end = 100},
	};

	zassert_equal(app_send_split(&pending, &m_ops, &m_fake), -EIO);

	/* Only the first message went out; the rest stays in the rings for the next report */
	zassert_equal(m_fake.sent_count, 1);
	zassert_equal(m_fake.sent[0].rec[0].first, 0);
	zassert_equal(m_fake.drop_count, 0);
}

ZTEST(send_split, test_encode_error_propagated)
{
	m_fake.encode_error = -EFAULT;

	struct app_send_select pending = {
		.rec[0] = {.first = 0, .end = 10},
	};

	zassert_equal(app_send_split(&pending, &m_ops, &m_fake), -EFAULT);
	zassert_equal(m_fake.sent_count, 0);
	zassert_equal(m_fake.drop_count, 0);
}

ZTEST(send_split, test_nothing_pending)
{
	struct app_send_select pending = {0};

	zassert_ok(app_send_split(&pending, &m_ops, &m_fake));
	zassert_equal(m_fake.sent_count, 0);
}

ZTEST_SUITE(send_split, NULL, NULL, before, NULL, NULL);
