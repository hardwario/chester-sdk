/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_util.h"
#include "ctr_cloud_msg.h"
#include "ctr_cloud_config.h"
#include "ctr_cloud_spool.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_cloud_shell, CONFIG_CTR_CLOUD_LOG_LEVEL);

static void print_ts(const struct shell *shell, const char *name, int64_t ts, int64_t now)
{
	if (ts < 0) {
		shell_print(shell, "%s: -1 (never)", name);
		return;
	}

	/* Check if timestamp is before 2020 (time not synced yet) */
	if (ts < 1577836800LL || now < 1577836800LL) {
		shell_print(shell, "%s: %lld (not synced)", name, ts);
		return;
	}

	int64_t ago_sec = now - ts;

	if (ago_sec < 0) {
		ago_sec = 0;
	}

	int hours = ago_sec / 3600;
	int mins = (ago_sec % 3600) / 60;
	int secs = ago_sec % 60;

	if (hours > 0) {
		shell_print(shell, "%s: %lld (%d:%02d:%02d ago)", name, ts, hours, mins, secs);
	} else {
		shell_print(shell, "%s: %lld (%d:%02d ago)", name, ts, mins, secs);
	}
}

static int cmd_state(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	bool initialized;
	ret = ctr_cloud_is_initialized(&initialized);
	if (ret) {
		shell_error(shell, "ctr_cloud_is_initialized failed: %d", ret);
		return ret;
	}

	shell_print(shell, "initialized: %s", initialized ? "yes" : "no");

	int64_t last_seen = 0;
	ctr_cloud_get_last_seen_ts(&last_seen);
	shell_print(shell, "last seen ts: %lld", last_seen);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_metrics(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	struct ctr_cloud_metrics metrics;
	ret = ctr_cloud_get_metrics(&metrics);
	if (ret) {
		shell_error(shell, "ctr_cloud_get_metrics failed: %d", ret);
		return ret;
	}

	int64_t now = metrics.timestamp;

	shell_print(shell, "uplink count: %u", metrics.uplink_count);
	shell_print(shell, "uplink fragments: %u", metrics.uplink_fragments);
	shell_print(shell, "uplink bytes: %u", metrics.uplink_bytes);
	print_ts(shell, "uplink last ts", metrics.uplink_last_ts, now);
	shell_print(shell, "uplink errors: %u", metrics.uplink_errors);
	print_ts(shell, "uplink error last ts", metrics.uplink_error_last_ts, now);
	shell_print(shell, "downlink count: %u", metrics.downlink_count);
	shell_print(shell, "downlink fragments: %u", metrics.downlink_fragments);
	shell_print(shell, "downlink bytes: %u", metrics.downlink_bytes);
	print_ts(shell, "downlink last ts", metrics.downlink_last_ts, now);
	shell_print(shell, "downlink errors: %u", metrics.downlink_errors);
	print_ts(shell, "downlink error last ts", metrics.downlink_error_last_ts, now);
	shell_print(shell, "poll count: %u", metrics.poll_count);
	print_ts(shell, "poll last ts", metrics.poll_last_ts, now);
	shell_print(shell, "uplink data count: %u", metrics.uplink_data_count);
	print_ts(shell, "uplink data last ts", metrics.uplink_data_last_ts, now);
	shell_print(shell, "downlink data count: %u", metrics.downlink_data_count);
	print_ts(shell, "downlink data last ts", metrics.downlink_data_last_ts, now);
	shell_print(shell, "recv shell count: %u", metrics.recv_shell_count);
	print_ts(shell, "recv shell last ts", metrics.recv_shell_last_ts, now);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_firmware_download(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		shell_error(shell, "missing firmware id");
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_cloud_firmware_update(argv[1]);
	if (ret) {
		shell_error(shell, "ctr_cloud_firmware_update failed: %d", ret);
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_poll(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_cloud_poll_immediately();
	if (ret) {
		shell_error(shell, "ctr_cloud_poll_immediately failed: %d", ret);
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

#if defined(CONFIG_CTR_CLOUD_SPOOL)

static int cmd_spool_count(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int count;
	ret = ctr_cloud_spool_count(&count);
	if (ret) {
		shell_error(shell, "ctr_cloud_spool_count failed: %d", ret);
		return ret;
	}

	shell_print(shell, "spool messages: %d", count);

	return 0;
}

static int cmd_spool_fake(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	long n = strtol(argv[1], NULL, 10);
	if (n < 1 || n > 100) {
		shell_error(shell, "invalid count (expected 1-100)");
		return -EINVAL;
	}

	for (long i = 1; i <= n; i++) {
		CTR_BUF_DEFINE(buf, 1 + 8 + 16);

		ctr_buf_append_u8(&buf, UL_UPLOAD_DATA);
		ctr_buf_append_u64_be(&buf, 0); /* fake decoder hash */

		/* 16 payload bytes, all set to the fake message number */
		for (int j = 0; j < 16; j++) {
			ctr_buf_append_u8(&buf, (uint8_t)i);
		}

		ret = ctr_cloud_spool_save(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
		if (ret) {
			shell_error(shell, "ctr_cloud_spool_save failed: %d", ret);
			return ret;
		}

		/* Filenames have millisecond resolution - keep them unique */
		k_sleep(K_MSEC(2));
	}

	shell_print(shell, "created %ld fake message(s)", n);

	return 0;
}

static int cmd_spool_clean(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = ctr_cloud_spool_clear();
	if (ret) {
		shell_error(shell, "ctr_cloud_spool_clear failed: %d", ret);
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

#endif /* defined(CONFIG_CTR_CLOUD_SPOOL) */

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cloud_firmware,

			       SHELL_CMD_ARG(download, NULL,
					     "Download firmware. (format: <fw-id>).",
					     cmd_firmware_download, 2, 0),

			       SHELL_SUBCMD_SET_END);

#if defined(CONFIG_CTR_CLOUD_SPOOL)

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cloud_spool,

			       SHELL_CMD_ARG(count, NULL, "Get number of spooled messages.",
					     cmd_spool_count, 1, 0),
			       SHELL_CMD_ARG(fake, NULL,
					     "Create fake spooled messages (format: <count>).",
					     cmd_spool_fake, 2, 0),
			       SHELL_CMD_ARG(clean, NULL, "Delete all spooled messages.",
					     cmd_spool_clean, 1, 0),

			       SHELL_SUBCMD_SET_END);

#endif /* defined(CONFIG_CTR_CLOUD_SPOOL) */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_cloud,
#if defined(CONFIG_CTR_CLOUD_CONFIG)
	SHELL_CMD_ARG(config, NULL, "Configuration commands.", ctr_cloud_config_cmd, 1, 3),
#endif
#if defined(CONFIG_CTR_CLOUD_SPOOL)
	SHELL_CMD_ARG(spool, &sub_cloud_spool, "Spool commands.", print_help, 1, 0),
#endif
	SHELL_CMD_ARG(state, NULL, "Get CLOUD state.", cmd_state, 1, 0),
	SHELL_CMD_ARG(metrics, NULL, "Get CLOUD metrics.", cmd_metrics, 1, 0),
	SHELL_CMD_ARG(poll, NULL, "Poll CLOUD.", cmd_poll, 1, 0),
	SHELL_CMD_ARG(firmware, &sub_cloud_firmware, "Firmware commands.", print_help, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(cloud, &sub_cloud, "CLOUD commands.", print_help);
