/*
 * Copyright (c) 2023 HARDWARIO a.s.
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

	struct ctr_lte_v2_cereg_param cereg_param;
	ctr_lte_v2_get_cereg_param(&cereg_param);
	if (cereg_param.valid) {
		switch (cereg_param.stat) {
		case CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED:
			shell_print(shell, "cereg: not registered");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME:
			shell_print(shell, "cereg: registered home");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING:
			shell_print(shell, "cereg: searching");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED:
			shell_print(shell, "cereg: registration denied");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_UNKNOWN:
			shell_print(shell, "cereg: unknown");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING:
			shell_print(shell, "cereg: registered roaming");
			break;
		case CTR_LTE_V2_CEREG_PARAM_STAT_SIM_FAILURE:
			shell_print(shell, "cereg: sim failure");
			break;
		default:
			shell_print(shell, "cereg: unknown");
			break;
		}

		switch (cereg_param.act) {
		case CTR_LTE_V2_CEREG_PARAM_ACT_LTE:
			shell_print(shell, "mode: lte-m");
			break;
		case CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT:
			shell_print(shell, "mode: nb-iot");
			break;
		case CTR_LTE_V2_CEREG_PARAM_ACT_UNKNOWN:
		default:
			shell_print(shell, "act: unknown");
			break;
		}
	}

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

	shell_print(shell, "state: %s", ctr_lte_v2_get_state());

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

	shell_print(shell, "uplink messages: %u", metrics.uplink_count);
	shell_print(shell, "uplink bytes: %u", metrics.uplink_bytes);
	shell_print(shell, "uplink errors: %u", metrics.uplink_errors);
	shell_print(shell, "uplink last ts: %lld", metrics.uplink_last_ts);
	shell_print(shell, "downlink messages: %u", metrics.downlink_count);
	shell_print(shell, "downlink bytes: %u", metrics.downlink_bytes);
	shell_print(shell, "downlink errors: %u", metrics.downlink_errors);
	shell_print(shell, "downlink last ts: %lld", metrics.downlink_last_ts);

	shell_print(shell, "command succeeded");

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
	              ctr_lte_v2_flow_cmd_test_uart, 2, 0),

	SHELL_CMD_ARG(reset, NULL,
	              "Reset modem.",
	              ctr_lte_v2_flow_cmd_test_reset, 1, 0),

	SHELL_CMD_ARG(wakeup, NULL,
	              "Wake up modem.",
	              ctr_lte_v2_flow_cmd_test_wakeup, 1, 0),

	SHELL_CMD_ARG(cmd, NULL,
	              "Send command to modem. (format: <command>)",
	              ctr_lte_v2_flow_cmd_test_cmd, 2, 0),

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

	SHELL_CMD_ARG(test, &sub_lte_test,
	              "Test commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);
