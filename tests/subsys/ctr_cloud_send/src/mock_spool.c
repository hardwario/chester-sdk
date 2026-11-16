/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Mock spool backend in RAM, with the semantics of ctr_cloud_spool.h (oldest first,
 * load appends, ids never reused) plus counters and error injection.
 */

#include "common.h"
#include "ctr_cloud_spool.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#define SLOTS     8
#define SLOT_SIZE 64

struct slot {
	bool used;
	ctr_cloud_spool_id id;
	size_t len;
	uint8_t mem[SLOT_SIZE];
};

static struct slot m_slots[SLOTS];
static ctr_cloud_spool_id m_next_id = 1;
static int m_save_count;
static int m_delete_count;
static int m_load_count;
static int m_fail_next_save;
static int m_fail_next_delete;
static uint8_t m_last_saved[SLOT_SIZE];
static size_t m_last_saved_len;

void mock_spool_reset(void)
{
	memset(m_slots, 0, sizeof(m_slots));
	m_save_count = 0;
	m_delete_count = 0;
	m_load_count = 0;
	m_fail_next_save = 0;
	m_fail_next_delete = 0;
	m_last_saved_len = 0;
}

void mock_spool_fail_next_save(int err)
{
	m_fail_next_save = err;
}

void mock_spool_fail_next_delete(int err)
{
	m_fail_next_delete = err;
}

int mock_spool_save_count(void)
{
	return m_save_count;
}

int mock_spool_delete_count(void)
{
	return m_delete_count;
}

int mock_spool_load_count(void)
{
	return m_load_count;
}

const uint8_t *mock_spool_last_saved(size_t *len)
{
	*len = m_last_saved_len;

	return m_last_saved;
}

int mock_spool_stored(void)
{
	int count = 0;

	for (int i = 0; i < SLOTS; i++) {
		count += m_slots[i].used;
	}

	return count;
}

static struct slot *find(ctr_cloud_spool_id id)
{
	for (int i = 0; i < SLOTS; i++) {
		if (m_slots[i].used && m_slots[i].id == id) {
			return &m_slots[i];
		}
	}

	return NULL;
}

static struct slot *oldest(void)
{
	struct slot *found = NULL;

	for (int i = 0; i < SLOTS; i++) {
		if (m_slots[i].used && (!found || m_slots[i].id < found->id)) {
			found = &m_slots[i];
		}
	}

	return found;
}

int ctr_cloud_spool_save(const void *buf, size_t len, ctr_cloud_spool_id *id)
{
	if (m_fail_next_save) {
		int err = m_fail_next_save;

		m_fail_next_save = 0;
		return err;
	}

	if (!len || len > SLOT_SIZE) {
		return -EFBIG;
	}

	struct slot *s = NULL;

	for (int i = 0; i < SLOTS; i++) {
		if (!m_slots[i].used) {
			s = &m_slots[i];
			break;
		}
	}

	if (!s) {
		/* Full - drop the oldest message */
		s = oldest();
	}

	s->used = true;
	s->id = m_next_id++;
	s->len = len;
	memcpy(s->mem, buf, len);

	memcpy(m_last_saved, buf, len);
	m_last_saved_len = len;
	m_save_count++;

	if (id) {
		*id = s->id;
	}

	return 0;
}

int ctr_cloud_spool_peek(ctr_cloud_spool_id *id)
{
	struct slot *s = oldest();

	if (!s) {
		return 1;
	}

	*id = s->id;

	return 0;
}

int ctr_cloud_spool_load(ctr_cloud_spool_id id, struct ctr_buf *buf)
{
	struct slot *s = find(id);

	if (!s) {
		return -ENOENT;
	}

	m_load_count++;

	return ctr_buf_append_mem(buf, s->mem, s->len) ? -EFBIG : 0;
}

int ctr_cloud_spool_delete(ctr_cloud_spool_id id)
{
	if (m_fail_next_delete) {
		int err = m_fail_next_delete;

		m_fail_next_delete = 0;
		return err;
	}

	struct slot *s = find(id);

	if (!s) {
		return -ENOENT;
	}

	s->used = false;
	m_delete_count++;

	return 0;
}

int ctr_cloud_spool_count(int *count)
{
	*count = mock_spool_stored();

	return 0;
}

int ctr_cloud_spool_clear(void)
{
	memset(m_slots, 0, sizeof(m_slots));

	return 0;
}
