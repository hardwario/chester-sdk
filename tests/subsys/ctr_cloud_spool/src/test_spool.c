/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Spool LittleFS backend tests - run against a real LittleFS mounted on the
 * flash simulator; only the RTC and the cloud config are mocked.
 */

#include "ctr_cloud_config.h"
#include "ctr_cloud_spool.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Zephyr includes */
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

#define DIR_SPOOL "/lfs1/spool"

/* Base timestamp [ms]: 2026-01-01T00:00:00.000Z */
#define TS_BASE 1767225600000LL

/* Mocks ----------------------------------------------------------------------*/

struct ctr_cloud_config g_ctr_cloud_config;

static int64_t m_ts_ms;

int ctr_rtc_get_ts_ms(int64_t *ts_ms)
{
	*ts_ms = m_ts_ms;
	return 0;
}

/* Fixture --------------------------------------------------------------------*/

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(m_lfs_data);

static struct fs_mount_t m_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &m_lfs_data,
	/* No MCUboot in the test - reuse the largest native_sim partition */
	.storage_dev = (void *)PARTITION_ID(slot1_partition),
	.mnt_point = "/lfs1",
};

static void *setup(void)
{
	const struct flash_area *fa;

	/* The flash simulator persists in flash.bin - start from scratch */
	zassert_ok(flash_area_open(PARTITION_ID(slot1_partition), &fa));
	zassert_ok(flash_area_erase(fa, 0, fa->fa_size));
	flash_area_close(fa);

	zassert_ok(fs_mount(&m_mnt));

	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	zassert_ok(ctr_cloud_spool_clear());

	g_ctr_cloud_config.spool_size = 10;
	m_ts_ms = TS_BASE;
}

static int count(void)
{
	int count;
	zassert_ok(ctr_cloud_spool_count(&count));
	return count;
}

static void save(const char *msg, ctr_cloud_spool_id *id)
{
	zassert_ok(ctr_cloud_spool_save(msg, strlen(msg), id));
}

/* Load the message `id` into a fresh buffer and check its content */
static void assert_content(ctr_cloud_spool_id id, const char *msg)
{
	CTR_BUF_DEFINE(buf, 64);

	zassert_ok(ctr_cloud_spool_load(id, &buf));
	zassert_equal(ctr_buf_get_used(&buf), strlen(msg));
	zassert_mem_equal(ctr_buf_get_mem(&buf), msg, strlen(msg));
}

/* Create a file in the spool directory bypassing the spool API */
static void create_raw_file(const char *name, const void *data, size_t len)
{
	char path[64];
	struct fs_file_t file;
	struct fs_dirent entry;

	if (fs_stat(DIR_SPOOL, &entry)) {
		zassert_ok(fs_mkdir(DIR_SPOOL));
	}

	snprintf(path, sizeof(path), DIR_SPOOL "/%s", name);

	fs_file_t_init(&file);
	zassert_ok(fs_open(&file, path, FS_O_CREATE | FS_O_WRITE));
	if (len) {
		zassert_equal(fs_write(&file, data, len), len);
	}
	zassert_ok(fs_close(&file));
}

/* Tests ----------------------------------------------------------------------*/

ZTEST(ctr_cloud_spool, test_roundtrip)
{
	ctr_cloud_spool_id id, peeked;

	save("hello", &id);
	zassert_equal(id, TS_BASE);
	zassert_equal(count(), 1);

	zassert_equal(ctr_cloud_spool_peek(&peeked), 0);
	zassert_equal(peeked, id);
	assert_content(id, "hello");

	zassert_ok(ctr_cloud_spool_delete(id));
	zassert_equal(ctr_cloud_spool_peek(&peeked), 1, "spool should be empty");
}

ZTEST(ctr_cloud_spool, test_load_appends_to_buffer)
{
	ctr_cloud_spool_id id;
	CTR_BUF_DEFINE(buf, 64);

	save("payload", &id);

	zassert_ok(ctr_buf_append_mem(&buf, (const uint8_t *)"hdr", 3));
	zassert_ok(ctr_cloud_spool_load(id, &buf));

	zassert_equal(ctr_buf_get_used(&buf), 10);
	zassert_mem_equal(ctr_buf_get_mem(&buf), "hdrpayload", 10);
}

ZTEST(ctr_cloud_spool, test_peek_returns_oldest)
{
	ctr_cloud_spool_id id;

	m_ts_ms = TS_BASE + 3000;
	save("third", NULL);
	m_ts_ms = TS_BASE + 1000;
	save("first", NULL);
	m_ts_ms = TS_BASE + 2000;
	save("second", NULL);

	zassert_equal(ctr_cloud_spool_peek(&id), 0);
	zassert_equal(id, TS_BASE + 1000);
	assert_content(id, "first");
}

ZTEST(ctr_cloud_spool, test_same_ms_keeps_both_messages)
{
	ctr_cloud_spool_id id_a, id_b, peeked;

	/* RTC restarts from the same base after a reset without time sync, and
	 * the sub-second part saturates at 999 ms - equal timestamps happen */
	save("message A", &id_a);
	save("message B", &id_b);

	zassert_equal(count(), 2, "second save must not overwrite the first");
	zassert_true(id_b > id_a, "the later message must sort after the earlier one");

	zassert_equal(ctr_cloud_spool_peek(&peeked), 0);
	zassert_equal(peeked, id_a);
	assert_content(id_a, "message A");

	zassert_ok(ctr_cloud_spool_delete(id_a));

	zassert_equal(ctr_cloud_spool_peek(&peeked), 0);
	zassert_equal(peeked, id_b);
	assert_content(id_b, "message B");
}

ZTEST(ctr_cloud_spool, test_full_drops_oldest)
{
	ctr_cloud_spool_id id;

	g_ctr_cloud_config.spool_size = 3;

	for (int i = 0; i < 4; i++) {
		m_ts_ms = TS_BASE + i * 1000;
		save("msg", NULL);
	}

	zassert_equal(count(), 3);
	zassert_equal(ctr_cloud_spool_peek(&id), 0);
	zassert_equal(id, TS_BASE + 1000, "the oldest message should have been dropped");
}

ZTEST(ctr_cloud_spool, test_disabled)
{
	g_ctr_cloud_config.spool_size = 0;

	zassert_equal(ctr_cloud_spool_save("msg", 3, NULL), -ENOSPC);
	zassert_equal(count(), 0);
}

ZTEST(ctr_cloud_spool, test_clear_removes_all)
{
	for (int i = 0; i < 7; i++) {
		m_ts_ms = TS_BASE + i * 1000;
		save("msg", NULL);
	}
	zassert_equal(count(), 7);

	zassert_ok(ctr_cloud_spool_clear());
	zassert_equal(count(), 0);
}

ZTEST(ctr_cloud_spool, test_load_empty_file_fails)
{
	CTR_BUF_DEFINE(buf, 64);

	/* Power loss between create and close leaves an empty file behind */
	create_raw_file("2026-01-01T00-00-00-000Z", NULL, 0);

	zassert_equal(ctr_cloud_spool_load(TS_BASE, &buf), -ENODATA);
	zassert_equal(ctr_buf_get_used(&buf), 0);
}

ZTEST(ctr_cloud_spool, test_load_oversized_file_fails)
{
	ctr_cloud_spool_id id;
	CTR_BUF_DEFINE(buf, 8);

	save("longer than eight bytes", &id);

	/* A truncated frame must not be sent */
	zassert_equal(ctr_cloud_spool_load(id, &buf), -EFBIG);
	zassert_equal(ctr_buf_get_used(&buf), 0);
}

ZTEST(ctr_cloud_spool, test_failed_write_leaves_no_file)
{
	/* Bigger than the whole partition - the write must fail */
	static uint8_t big[512 * 1024];

	memset(big, 0xaa, sizeof(big));

	zassert_true(ctr_cloud_spool_save(big, sizeof(big), NULL) < 0);
	zassert_equal(count(), 0, "a partially written message must be removed");
}

ZTEST_SUITE(ctr_cloud_spool, NULL, setup, before, NULL, NULL);
