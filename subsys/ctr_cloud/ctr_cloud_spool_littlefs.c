/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_config.h"
#include "ctr_cloud_spool.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>

/* Standard includes */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

LOG_MODULE_REGISTER(ctr_cloud_spool, CONFIG_CTR_CLOUD_LOG_LEVEL);

/*
 * LittleFS spool backend. The message identifier is the UTC timestamp in
 * milliseconds; the file name is derived from it deterministically, so
 * id <-> file name conversion needs no lookup table.
 */

#define DIR_SPOOL "/lfs1/spool"

#define FILENAME_SIZE 32

static int ensure_dir(void)
{
	int ret;
	struct fs_dirent entry;

	ret = fs_stat(DIR_SPOOL, &entry);
	if (!ret && entry.type == FS_DIR_ENTRY_DIR) {
		return 0;
	}

	ret = fs_mkdir(DIR_SPOOL);
	if (ret) {
		LOG_ERR("Call `fs_mkdir` failed: %d", ret);
		return ret;
	}

	LOG_INF("Created directory " DIR_SPOOL);

	return 0;
}

static void make_filename(ctr_cloud_spool_id id, char *name, size_t name_size)
{
	time_t ts = id / 1000;
	struct tm tm;
	gmtime_r(&ts, &tm);

	/* ISO-8601-like name (dashes instead of colons); lexicographic order
	 * equals chronological order, milliseconds avoid same-second collisions */
	snprintf(name, name_size, "%04d-%02d-%02dT%02d-%02d-%02d-%03dZ", tm.tm_year + 1900,
		 tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(id % 1000));
}

static int parse_filename(const char *name, ctr_cloud_spool_id *id)
{
	struct tm tm = {0};
	int ms;

	if (sscanf(name, "%4d-%2d-%2dT%2d-%2d-%2d-%3dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &ms) != 7) {
		return -EINVAL;
	}

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	*id = (ctr_cloud_spool_id)timeutil_timegm64(&tm) * 1000 + ms;

	return 0;
}

int ctr_cloud_spool_save(const void *buf, size_t len, ctr_cloud_spool_id *id)
{
	int ret;
	struct fs_file_t file;

	if (g_ctr_cloud_config.spool_size <= 0) {
		LOG_DBG("Spool is disabled (spool-size is 0)");
		return -ENOSPC;
	}

	ret = ensure_dir();
	if (ret) {
		return ret;
	}

	int count;
	ret = ctr_cloud_spool_count(&count);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_spool_count` failed: %d", ret);
		return ret;
	}

	/* Spool full - drop the oldest message(s) to make room for the newest */
	while (count >= g_ctr_cloud_config.spool_size) {
		ctr_cloud_spool_id oldest;
		ret = ctr_cloud_spool_peek(&oldest);
		if (ret) {
			break;
		}

		ret = ctr_cloud_spool_delete(oldest);
		if (ret) {
			return ret;
		}

		LOG_WRN("Spool full - dropped oldest message: %llu", oldest);

		count--;
	}

	int64_t ts_ms;
	ret = ctr_rtc_get_ts_ms(&ts_ms);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts_ms` failed: %d", ret);
		return ret;
	}

	char name[FILENAME_SIZE];
	make_filename(ts_ms, name, sizeof(name));

	char filepath[64];
	snprintf(filepath, sizeof(filepath), DIR_SPOOL "/%s", name);

	fs_file_t_init(&file);

	ret = fs_open(&file, filepath, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret < 0) {
		LOG_ERR("Call `fs_open` failed: %d", ret);
		return ret;
	}

	ret = fs_write(&file, buf, len);
	if (ret < 0) {
		LOG_ERR("Call `fs_write` failed: %d", ret);
		fs_close(&file);
		return ret;
	}

	if ((size_t)ret < len) {
		LOG_ERR("Incomplete write (%d of %u bytes)", ret, (unsigned int)len);
		fs_close(&file);
		fs_unlink(filepath);
		return -ENOSPC;
	}

	fs_close(&file);

	if (id) {
		*id = ts_ms;
	}

	LOG_INF("Stored %u bytes to %s", (unsigned int)len, filepath);

	return 0;
}

int ctr_cloud_spool_peek(ctr_cloud_spool_id *id)
{
	int ret;
	struct fs_dir_t dir;
	struct fs_dirent entry;
	char oldest[FILENAME_SIZE];
	bool found = false;

	fs_dir_t_init(&dir);

	ret = fs_opendir(&dir, DIR_SPOOL);
	if (ret == -ENOENT) {
		return 1;
	} else if (ret < 0) {
		LOG_ERR("Call `fs_opendir` failed: %d", ret);
		return ret;
	}

	while (!fs_readdir(&dir, &entry) && entry.name[0]) {
		if (entry.type != FS_DIR_ENTRY_FILE) {
			continue;
		}

		ctr_cloud_spool_id entry_id;
		if (parse_filename(entry.name, &entry_id)) {
			LOG_WRN("Skipping foreign file: %s", entry.name);
			continue;
		}

		/* Names are timestamps - lexicographic minimum is the oldest
		 * message (readdir order is not sorted) */
		if (!found || strcmp(entry.name, oldest) < 0) {
			strncpy(oldest, entry.name, sizeof(oldest) - 1);
			oldest[sizeof(oldest) - 1] = '\0';
			found = true;
		}
	}

	fs_closedir(&dir);

	if (found) {
		return parse_filename(oldest, id);
	}

	return 1;
}

int ctr_cloud_spool_load(ctr_cloud_spool_id id, struct ctr_buf *buf)
{
	int ret;
	struct fs_file_t file;

	char name[FILENAME_SIZE];
	make_filename(id, name, sizeof(name));

	char filepath[64];
	snprintf(filepath, sizeof(filepath), DIR_SPOOL "/%s", name);

	fs_file_t_init(&file);

	ret = fs_open(&file, filepath, FS_O_READ);
	if (ret < 0) {
		LOG_ERR("Call `fs_open` failed: %d", ret);
		return ret;
	}

	size_t used = ctr_buf_get_used(buf);

	ret = fs_read(&file, ctr_buf_get_mem(buf) + used, ctr_buf_get_free(buf));
	if (ret < 0) {
		LOG_ERR("Call `fs_read` failed: %d", ret);
		fs_close(&file);
		return ret;
	}

	fs_close(&file);

	size_t len = ret;

	ret = ctr_buf_seek(buf, used + len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	LOG_INF("Loaded %u bytes from %s", (unsigned int)len, filepath);

	return 0;
}

int ctr_cloud_spool_delete(ctr_cloud_spool_id id)
{
	int ret;

	char name[FILENAME_SIZE];
	make_filename(id, name, sizeof(name));

	char filepath[64];
	snprintf(filepath, sizeof(filepath), DIR_SPOOL "/%s", name);

	ret = fs_unlink(filepath);
	if (ret < 0) {
		LOG_ERR("Call `fs_unlink` failed: %d", ret);
		return ret;
	}

	LOG_INF("Deleted %s", filepath);

	return 0;
}

int ctr_cloud_spool_count(int *count)
{
	int ret;
	struct fs_dir_t dir;
	struct fs_dirent entry;

	*count = 0;

	fs_dir_t_init(&dir);

	ret = fs_opendir(&dir, DIR_SPOOL);
	if (ret == -ENOENT) {
		return 0;
	} else if (ret < 0) {
		LOG_ERR("Call `fs_opendir` failed: %d", ret);
		return ret;
	}

	while (!fs_readdir(&dir, &entry) && entry.name[0]) {
		if (entry.type == FS_DIR_ENTRY_FILE) {
			(*count)++;
		}
	}

	fs_closedir(&dir);

	return 0;
}

int ctr_cloud_spool_clear(void)
{
	int ret;
	struct fs_dir_t dir;
	struct fs_dirent entry;

	fs_dir_t_init(&dir);

	ret = fs_opendir(&dir, DIR_SPOOL);
	if (ret == -ENOENT) {
		return 0;
	} else if (ret < 0) {
		LOG_ERR("Call `fs_opendir` failed: %d", ret);
		return ret;
	}

	/* Delete directly by directory listing (not via peek) so that foreign
	 * files are removed as well */
	while (!fs_readdir(&dir, &entry) && entry.name[0]) {
		if (entry.type != FS_DIR_ENTRY_FILE) {
			continue;
		}

		char filepath[64];
		snprintf(filepath, sizeof(filepath), DIR_SPOOL "/%.48s", entry.name);

		ret = fs_unlink(filepath);
		if (ret < 0) {
			LOG_ERR("Call `fs_unlink` failed: %d", ret);
			fs_closedir(&dir);
			return ret;
		}

		LOG_INF("Deleted %s", filepath);
	}

	fs_closedir(&dir);

	return 0;
}
