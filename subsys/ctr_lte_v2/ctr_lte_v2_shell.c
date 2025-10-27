/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_state.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2_shell, CONFIG_CTR_LTE_V2_LOG_LEVEL);

static int cmd_imei(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	uint64_t imei;
	ret = ctr_lte_v2_get_imei(&imei);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_imei` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "imei: %llu", imei);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_imsi(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	uint64_t imsi;
	ret = ctr_lte_v2_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_imsi` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "imsi: %llu", imsi);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_iccid(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	char *iccid;
	ret = ctr_lte_v2_get_iccid(&iccid);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_iccid` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "iccid: %s", iccid);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_fw_version(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	char *version;
	ret = ctr_lte_v2_get_modem_fw_version(&version);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_modem_fw_version` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "fw-version: %s", version);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_state(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	bool attached;
	ret = ctr_lte_v2_is_attached(&attached);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_is_attached` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "attached: %s", attached ? "yes" : "no");

	const char *lte_cereg = "not available";
	const char *lte_mode = "not available";
	struct ctr_lte_v2_cereg_param cereg_param;
	ctr_lte_v2_get_cereg_param(&cereg_param);
	if (cereg_param.valid) {
		lte_cereg = ctr_lte_v2_str_cereg_stat(cereg_param.stat);
		lte_mode = ctr_lte_v2_str_act(cereg_param.act);
	}
	shell_print(shell, "cereg: %s", lte_cereg);
	shell_print(shell, "mode: %s", lte_mode);

	struct ctr_lte_v2_conn_param conn_param;
	ctr_lte_v2_get_conn_param(&conn_param);
	if (conn_param.valid) {
		shell_print(shell, "eest: %d", conn_param.eest);
		shell_print(shell, "ecl: %d", conn_param.ecl);
		shell_print(shell, "rsrp: %d", conn_param.rsrp);
		shell_print(shell, "rsrq: %d", conn_param.rsrq);
		shell_print(shell, "snr: %d", conn_param.snr);
		shell_print(shell, "plmn: %d", conn_param.plmn);
		shell_print(shell, "cid: %d", conn_param.cid);
		shell_print(shell, "band: %d", conn_param.band);
		shell_print(shell, "earfcn: %d", conn_param.earfcn);
	}

	shell_print(shell, "fsm-state: %s", ctr_lte_v2_get_state());
	if (strcmp(ctr_lte_v2_get_state(), "attach") == 0) {
		int attempt, attach_timeout_sec, remaining_sec;
		if (!ctr_lte_v2_get_curr_attach_info(&attempt, &attach_timeout_sec, NULL,
						     &remaining_sec)) {
			shell_print(shell, "attach-attempt: %d", attempt);
			shell_print(shell, "attach-timeout: %d:%02d", attach_timeout_sec / 60,
				    attach_timeout_sec % 60);
			shell_print(shell, "attach-remaining: %d:%02d", remaining_sec / 60,
				    remaining_sec % 60);
		}
	} else if (strcmp(ctr_lte_v2_get_state(), "retry_delay") == 0) {
		int attempt, retry_delay_sec, remaining_sec;
		if (!ctr_lte_v2_get_curr_attach_info(&attempt, NULL, &retry_delay_sec,
						     &remaining_sec)) {
			shell_print(shell, "attach-attempt: %d", attempt);
			shell_print(shell, "retry-delay-timeout: %d:%02d", retry_delay_sec / 60,
				    retry_delay_sec % 60);
			shell_print(shell, "retry-delay-remaining: %d:%02d", remaining_sec / 60,
				    remaining_sec % 60);
		}
	}

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

	struct ctr_lte_v2_metrics metrics;
	ret = ctr_lte_v2_get_metrics(&metrics);
	if (ret) {
		shell_error(shell, "ctr_lte_v2_get_metrics failed: %d", ret);
		return ret;
	}

	shell_print(shell, "attach count: %u", metrics.attach_count);
	shell_print(shell, "attach fail count: %u", metrics.attach_fail_count);
	shell_print(shell, "attach duration ms: %u", metrics.attach_duration_ms);
	shell_print(shell, "attach last ts: %lld", metrics.attach_last_ts);
	shell_print(shell, "attach last duration ms: %u", metrics.attach_last_duration_ms);

	shell_print(shell, "uplink messages: %u", metrics.uplink_count);
	shell_print(shell, "uplink bytes: %u", metrics.uplink_bytes);
	shell_print(shell, "uplink errors: %u", metrics.uplink_errors);
	shell_print(shell, "uplink last ts: %lld", metrics.uplink_last_ts);

	shell_print(shell, "downlink messages: %u", metrics.downlink_count);
	shell_print(shell, "downlink bytes: %u", metrics.downlink_bytes);
	shell_print(shell, "downlink errors: %u", metrics.downlink_errors);
	shell_print(shell, "downlink last ts: %lld", metrics.downlink_last_ts);

	shell_print(shell, "cscon 1 duration ms: %u", metrics.cscon_1_duration_ms);
	shell_print(shell, "cscon 1 last duration ms: %u", metrics.cscon_1_last_duration_ms);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_test_uart(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_error(shell, "command not found: %s", argv[2]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	if (strcmp(argv[1], "enable") == 0) {
		ret = ctr_lte_v2_flow_enable(false);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_enable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	if (strcmp(argv[1], "disable") == 0) {
		ret = ctr_lte_v2_flow_disable(false);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_disable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_test_reset(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_flow_reset();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_reset` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_test_wakeup(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_flow_wake_up_and_wait_on_ready();
	if (ret) {
		if (ret == -ENOTCONN) {
			shell_warn(shell, "uart not enabled");
			return 0;
		}
		if (ret == -EAGAIN) {
			shell_warn(shell, "boot message timed out");
			return 0;
		}

		LOG_ERR("Call `ctr_lte_v2_flow_wake_up_and_wait_on_ready` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_test_prepare(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_flow_prepare();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_prepare` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_test_cmd(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_error(shell, "only one argument is accepted (use quotes?)");
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_flow_cmd_without_response(argv[1]);
	if (ret) {
		if (ret == -ENOTCONN) {
			shell_warn(shell, "uart not enabled");
			return 0;
		}
		LOG_ERR("Call `ctr_lte_v2_flow_cmd_without_response` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static inline bool is_ascii(uint8_t data)
{
	return (data >= 0x30 && data <= 0x39) || (data >= 0x61 && data <= 0x66) ||
	       (data >= 0x41 && data <= 0x46);
}

static void flow_bypass_cb(void *user_data, const uint8_t *data, size_t len)
{
	// shell_print((const struct shell *)user_data, "%s", (const char *)data);
	const struct shell_fprintf *fprintf_ctx = (struct shell_fprintf *)user_data;
	fprintf_ctx->fwrite(fprintf_ctx->user_ctx, data, len);
}

static void shell_bypass_cb(const struct shell *shell, uint8_t *data, size_t len)
{
	static char line[256];
	static size_t line_len = 0;

	if (len == 0) {
		return;
	}

	if (strncmp((const char *)data, "+++", 3) == 0) {
		shell_print(shell, "exiting bypass mode");
		ctr_lte_v2_flow_bypass_set_cb(NULL, NULL);
		shell_set_bypass(shell, NULL);
		return;
	}

	for (size_t i = 0; i < len; i++) {
		if (data[i] == '\r' || data[i] == '\n') {
			line[line_len++] = '\r';
			line[line_len++] = '\n';
			line[line_len] = '\0';
			ctr_lte_v2_flow_bypass_write(line, line_len);
			line_len = 0;
			continue;
		}

		line[line_len++] = data[i];
		if (line_len >= sizeof(line)) {
			line_len = 0;
		}
	}
}

static int cmd_test_bypass(const struct shell *shell, size_t argc, char **argv)
{

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!g_ctr_lte_v2_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ctr_lte_v2_flow_bypass_set_cb(flow_bypass_cb, (void *)shell->fprintf_ctx);
	shell_set_bypass(shell, shell_bypass_cb);

	shell_print(shell, "bypass mode enabled, for exit type +++");

	return 0;
}

static int cmd_reconnect(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (g_ctr_lte_v2_config.test) {
		shell_error(shell, "not supported in test mode");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_reconnect();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_reconnect` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_info(shell, "command succeeded");

	return 0;
}

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

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte_test,

	SHELL_CMD_ARG(uart, NULL,
	              "Enable/Disable UART interface (format: <enable|disable>).",
	              cmd_test_uart, 2, 0),

	SHELL_CMD_ARG(reset, NULL,
	              "Reset modem.",
	              cmd_test_reset, 1, 0),

	SHELL_CMD_ARG(wakeup, NULL,
	              "Wake up modem.",
	              cmd_test_wakeup, 1, 0),

	SHELL_CMD_ARG(cmd, NULL,
	              "Send command to modem. (format: <command>).",
	              cmd_test_cmd, 2, 0),

	SHELL_CMD_ARG(prepare, NULL, "Run prepare modem sequence.", cmd_test_prepare, 1, 0),

	SHELL_CMD_ARG(bypass, NULL, "Switch to bypass mode.", cmd_test_bypass, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte,

	SHELL_CMD_ARG(config, NULL,
	              "Configuration commands.",
	              ctr_lte_v2_config_cmd, 1, 3),

	SHELL_CMD_ARG(imei, NULL,
	              "Get modem IMEI.",
	              cmd_imei, 1, 0),

	SHELL_CMD_ARG(imsi, NULL,
	              "Get SIM card IMSI.",
	              cmd_imsi, 1, 0),

	SHELL_CMD_ARG(iccid, NULL,
		      "Get SIM card ICCID.",
	              cmd_iccid, 1, 0),

	SHELL_CMD_ARG(fw-version, NULL,
	              "Get modem firmware version.",
	              cmd_fw_version, 1, 0),

	SHELL_CMD_ARG(state, NULL,
	              "Get LTE state.",
	              cmd_state, 1, 0),

	SHELL_CMD_ARG(metrics, NULL,
		     "Get LTE metrics.",
	              cmd_metrics, 1, 0),

	SHELL_CMD_ARG(reconnect, NULL,
	              "Reconnect LTE modem.",
	              cmd_reconnect, 1, 0),

	SHELL_CMD_ARG(test, &sub_lte_test,
	              "Test commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);
