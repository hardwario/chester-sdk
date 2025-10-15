/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdlib.h>

/* ### Preserved code "includes" (begin) */
#include <time.h>
#include <zephyr/sys/sys_heap.h>

#include <chester/ctr_cloud.h>
#include <chester/ctr_rtc.h>

#include "app_work.h"
#include "app_data.h"
#include "wmbus.h"
#include "packet.h"
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

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

/* ### Preserved code "functions 1" (begin) */
struct shell *m_shell = NULL;

struct shell *app_shell_get(void)
{
	return m_shell;
}

static int cmd_on(const struct shell *shell, size_t argc, char **argv)
{
	wmbus_enable();
	return 0;
}

static int cmd_off(const struct shell *shell, size_t argc, char **argv)
{
	wmbus_disable();
	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	return wmbus_configure(shell, argc, argv);
}

static int cmd_scan(const struct shell *shell, size_t argc, char **argv)
{
	m_shell = (struct shell *)shell;
	app_work_scan_trigger();
	return 0;
}

static int cmd_timeout(const struct shell *shell, size_t argc, char **argv)
{
	app_work_scan_timeout();
	return 0;
}

static int cmd_enroll(const struct shell *shell, size_t argc, char **argv)
{

	// wm enroll <timeout> <rssi_threshold>

	m_shell = (struct shell *)shell;

	int timeout = g_app_config.scan_timeout;
	int rssi_threshold = -150; // default is the weakest value

	if (argc >= 2) {
		timeout = atoi(argv[1]);
		if (timeout < 5 || timeout > 86400) {
			shell_print(shell, "Invalid timeout value, expected 5-86400 seconds");
			return -EINVAL;
		}
	}

	if (argc >= 3) {
		rssi_threshold = atoi(argv[2]);
		if (rssi_threshold < -128 || rssi_threshold > 0) {
			shell_print(shell, "Invalid RSSI threshold value, expected -128 to 0 dBm");
			return -EINVAL;
		}
	}

	shell_print(shell, "Enroll mode for %d seconds, RSSI threshold %d dBm", timeout,
		    rssi_threshold);

	app_work_scan_trigger_enroll(timeout, rssi_threshold);

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	// atomic_set(&g_app_data.send_trigger, true);
	app_work_send_trigger();
	return 0;
}

static int cmd_weekday(const struct shell *shell, size_t argc, char **argv)
{
	struct ctr_rtc_tm tm;
	ctr_rtc_get_tm(&tm);

	LOG_DBG("Weekday: %d", tm.wday);

	return 0;
}

int cmd_ant(const struct shell *shell, size_t argc, char **argv)
{
#if defined(CONFIG_SHIELD_CTR_B1)
	int ret;

	if (argc == 3) {

		int ant = atoi(argv[1]);
		int out = atoi(argv[2]);

		static const struct device *b1_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_b1));

		ret = ctr_b1_set_output(
			b1_dev, ant == 1 ? CTR_B1_OUTPUT_ANT_1 : CTR_B1_OUTPUT_ANT_2, out ? 1 : 0);
		if (ret) {
			LOG_ERR("Call `ctr_b1_set_output` failed: %d", ret);
		}

		return 0;
	}
#endif /* defined(CONFIG_SHIELD_CTR_B1) */

	shell_help(shell);
	return -EINVAL;
}

int cmd_packet_push(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	static const uint8_t packet_81763000[] = {
		0x33, 0x44, 0x68, 0x50, 0x00, 0x30, 0x76, 0x81, 0x69, 0x80, 0xa0, 0x91, 0x9f,
		0x2b, 0x06, 0x00, 0x70, 0x07, 0x00, 0x00, 0x61, 0x08, 0x7c, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
		0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12,
	};

	/*static const uint8_t packet_10726875[] = {
		0x34, 0x44, 0x68, 0x50, 0x75, 0x68, 0x72, 0x10, 0x94, 0x80, 0xa2, 0x0f,
	0x9f, 0x2d, 0x00, 0x00, 0xa0, 0x2f, 0x00, 0x00, 0x11, 0x0f, 0x08, 0xfa, 0x07, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
	};*/

	ret = packet_push((uint8_t *)packet_81763000, sizeof(packet_81763000));
	if (ret) {
		LOG_ERR("Call `packet_push` failed: %d", ret);
	}

	return 0;
}

int cmd_packet_poll(const struct shell *shell, size_t argc, char **argv)
{
	app_work_poll_trigger();

	return 0;
}

int cmd_packet_poll_ts(const struct shell *shell, size_t argc, char **argv)
{
#if defined(CONFIG_SHIELD_CTR_LTE_V2)

	int ret;

	int64_t timestamp;
	ret = ctr_cloud_get_last_seen_ts(&timestamp);

	if (ret) {
		shell_print(shell, "No downlink timestamp");
		return 0;
	}

	time_t t = timestamp;
	struct tm lt;
	char res[32];

	gmtime_r(&t, &lt);

	if (strftime(res, sizeof(res), "%a %b %d %Y %H:%M:%S", &lt) == 0) {
		LOG_ERR("Call `strftime` failed:");
		return -ETIME;
	}

	shell_print(shell, "ts: %llu, date: %s", (uint64_t)t, res);

#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */

	return 0;
}

extern struct sys_heap _system_heap;

int cmd_heap(const struct shell *shell, size_t argc, char **argv)
{
	struct sys_memory_stats stats;

	sys_heap_runtime_stats_get(&_system_heap, &stats);

	shell_print(shell, "allocated %zu, free %zu, max allocated %zu, heap size %u",
		    stats.allocated_bytes, stats.free_bytes, stats.max_allocated_bytes,
		    CONFIG_HEAP_MEM_POOL_SIZE);

	sys_heap_print_info(&_system_heap, true);

	return 0;
}
/* ^^^ Preserved code "functions 1" (end) */

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app,

			       SHELL_CMD_ARG(config, NULL, "Configurations commands.",
					     app_config_cmd_config, 1, 3),

/* ### Preserved code "subcmd" (begin) */
/* ^^^ Preserved code "subcmd" (end) */

			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);

/* ### Preserved code "functions 2" (begin) */
SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_wm,

	SHELL_CMD_ARG(on, NULL, "Enable wM-Bus receiver.", cmd_on, 0, 0),
	SHELL_CMD_ARG(off, NULL, "Disable wM-Bus receiver.", cmd_off, 0, 0),
	SHELL_CMD_ARG(config, NULL, "Config wM-Bus receiver.", cmd_config, 0, 0),

	SHELL_CMD_ARG(scan, NULL, "Trigger manual wM-Bus scan.", cmd_scan, 0, 0),
	SHELL_CMD_ARG(timeout, NULL, "Stop manual wM-Bus scan.", cmd_timeout, 0, 0),
	SHELL_CMD_ARG(enroll, NULL, "Scan and save sensors around. wm enroll <timeout> <rssi_threshold>.", cmd_enroll, 0, 3),

	SHELL_CMD_ARG(send, NULL, "Send data immediately.", cmd_send, 0, 0),
	SHELL_CMD_ARG(weekday, NULL, "Get current weekday.", cmd_weekday, 0, 0),

	SHELL_CMD_ARG(ant, NULL, "Manually control RF switch.", cmd_ant, 3, 0),

	SHELL_CMD_ARG(push, NULL, "Debug push packet to buffer.", cmd_packet_push, 0, 0),
	SHELL_CMD_ARG(poll, NULL, "Debug poll server for data.", cmd_packet_poll, 0, 0),
	SHELL_CMD_ARG(poll_ts, NULL, "Get timestamp of last downlink.", cmd_packet_poll_ts, 0, 0),

	SHELL_CMD_ARG(heap, NULL, "Show heap usage.", cmd_heap, 0, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(wm, &sub_wm, "wM-Bus debugging commands.", print_help);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
