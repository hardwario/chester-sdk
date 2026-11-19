/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_parse.h"
#include "ctr_lte_v2_state.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void print_cereg_event(const struct shell *shell, const char *label,
			      const struct ctr_lte_v2_cereg_event *e)
{
	char ago[24];

	ctr_util_fmt_duration(k_uptime_seconds() - e->uptime_s, ago, sizeof(ago));

	if (e->reject_cause) {
		shell_print(shell, "%s%s ago, %s, act: %s, tac: %04X, cell: %08X, emm: %u (%s)",
			    label, ago, ctr_lte_v2_str_cereg_stat(e->stat),
			    ctr_lte_v2_str_act(e->act), e->tac, e->cid, e->reject_cause,
			    ctr_lte_v2_str_emm_cause(e->reject_cause));
	} else {
		shell_print(shell, "%s%s ago, %s, act: %s, tac: %04X, cell: %08X", label, ago,
			    ctr_lte_v2_str_cereg_stat(e->stat), ctr_lte_v2_str_act(e->act), e->tac,
			    e->cid);
	}
}

static void print_last_reject(const struct shell *shell)
{
	struct ctr_lte_v2_cereg_event e;

	if (ctr_lte_v2_get_last_reject(&e)) {
		return;
	}

	print_cereg_event(shell, "last-reject: ", &e);

	const char *hint = ctr_lte_v2_str_emm_cause_hint(e.reject_cause);
	if (hint) {
		shell_print(shell, "last-reject-hint: %s", hint);
	}
}

static int cmd_history(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0
	struct ctr_lte_v2_cereg_event events[CONFIG_CTR_LTE_V2_CEREG_HISTORY];
	size_t count;

	int ret = ctr_lte_v2_get_cereg_history(events, ARRAY_SIZE(events), &count);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_cereg_history` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	/* Oldest first, the newest ends up next to the prompt */
	for (size_t i = count; i > 0; i--) {
		print_cereg_event(shell, "", &events[i - 1]);
	}
#endif

	print_last_reject(shell);

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

	/* The PSM the network granted, reported with a registration only */
	if (cereg_param.valid && cereg_param.active_time == GRPS_TIMER_DEACTIVATED) {
		shell_print(shell, "psm: not granted");
	} else if (cereg_param.valid && cereg_param.active_time >= 0 &&
		   cereg_param.periodic_tau_ext >= 0) {
		char active[24];
		char tau[24];

		ctr_util_fmt_duration(cereg_param.active_time, active, sizeof(active));
		ctr_util_fmt_duration(cereg_param.periodic_tau_ext, tau, sizeof(tau));
		shell_print(shell, "psm: active-time %s, periodic-tau %s", active, tau);
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

	char ceer[CTR_LTE_V2_STATE_CEER_SIZE];
	if (!ctr_lte_v2_state_get_ceer(ceer, sizeof(ceer))) {
		shell_print(shell, "ceer: %s", ceer);
	}

	print_last_reject(shell);

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

	ret = ctr_lte_v2_flow_prepare(g_ctr_lte_v2_config.mode);
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

static void shell_bypass_cb(const struct shell *shell, uint8_t *data, size_t len, void *user_data)
{
	static char line[256];
	static size_t line_len = 0;

	if (len == 0) {
		return;
	}

	if (strncmp((const char *)data, "+++", 3) == 0) {
		shell_print(shell, "exiting bypass mode");
		ctr_lte_v2_flow_bypass_set_cb(NULL, NULL);
		shell_set_bypass(shell, NULL, NULL);
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
	shell_set_bypass(shell, shell_bypass_cb, NULL);

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

#if defined(CONFIG_CTR_LTE_V2_SCAN)
/* The bound of 'lte ncellmeas now' */
#define NCELLMEAS_NOW_BOUND_S (10 * 60)

/* As hio: the status of the URC; an end without a readable one failed */
static void ncellmeas_result_str(const struct ctr_lte_v2_ncellmeas_param *param, char *buf,
				 size_t size)
{
	if (param->result || param->status == 1) {
		snprintf(buf, size, "failed");
	} else if (param->status) {
		snprintf(buf, size, "status %u", param->status);
	} else {
		snprintf(buf, size, "ok");
	}
}

static int print_ncellmeas(const struct shell *shell)
{
	/* A copy (on the stack): printing under the lock of the state would hold up the URCs */
	struct ctr_lte_v2_ncellmeas_param param;

	/* The cells of the previous measurement are gone once the new one is armed */
	if (ctr_lte_v2_wait_for_ncellmeas(K_NO_WAIT)) {
		shell_print(shell, "ncellmeas: in progress");
		shell_print(shell, "command succeeded");
		return 0;
	}

	int ret = ctr_lte_v2_get_ncellmeas_param(&param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_ncellmeas_param` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	if (param.result == -ENODATA) {
		shell_print(shell, "no measurement yet");
		shell_print(shell, "command succeeded");
		return 0;
	}

	char ago[24];
	char result[16];

	ctr_util_fmt_duration(k_uptime_seconds() - param.uptime_s, ago, sizeof(ago));
	ncellmeas_result_str(&param, result, sizeof(result));
	shell_print(shell, "ncellmeas: %s ago, %s", ago, result);

	for (int i = 0; i < param.num_cells; i++) {
		const struct ctr_lte_v2_ncellmeas_cell *c = &param.cells[i];

		shell_print(shell,
			    "cell: %08X, plmn: %s, tac: %04X, earfcn: %u, pci: %u, rsrp: %d dBm%s",
			    (unsigned int)c->eci, c->plmn, c->tac, (unsigned int)c->earfcn, c->pci,
			    ctr_lte_v2_parse_rsrp_dbm(c->rsrp), c->serving ? ", serving" : "");

		for (int j = 0; j < c->neighbor_count; j++) {
			const struct ctr_lte_v2_ncellmeas_ncell *n =
				&param.ncells[c->ncell_first + j];

			shell_print(shell, "  neighbour: earfcn: %u, pci: %u, rsrp: %d dBm",
				    (unsigned int)n->earfcn, n->pci,
				    ctr_lte_v2_parse_rsrp_dbm(n->rsrp));
		}
	}

	shell_print(shell, "command succeeded");

	return 0;
}

/* A line each 30 s from the start until wait() gives 0; -ETIMEDOUT at the bound */
static int ncellmeas_wait(const struct shell *shell, int (*wait)(k_timeout_t timeout))
{
	int64_t start = k_uptime_get();

	shell_print(shell, "measuring, this takes minutes...");

	while (wait(K_MSEC(30000 - (k_uptime_get() - start) % 30000))) {
		int64_t elapsed = (k_uptime_get() - start) / MSEC_PER_SEC;

		/* Beyond an attach from the radio off (5 min), the RRC idle (1 min) and the
		 * measurement (3 min): it waits for something else */
		if (elapsed >= NCELLMEAS_NOW_BOUND_S) {
			shell_error(shell, "measurement did not finish");
			return -ETIMEDOUT;
		}

		char took[24];

		ctr_util_fmt_duration(elapsed, took, sizeof(took));
		shell_print(shell, "still measuring (%s)", took);
	}

	return 0;
}

/* Test mode: no FSM, the command and its URC directly */
static int ncellmeas_now_test(const struct shell *shell)
{
	char err[32] = "";

	if (ctr_lte_v2_flow_test_link_up()) {
		shell_error(shell, "measurement not possible: modem not ready");
		return -EIO;
	}

	int ret = ctr_lte_v2_flow_test_ncellmeas_start(err, sizeof(err));
	if (ret) {
		if (err[0]) {
			shell_error(shell, "measurement not possible: %s", err);
			return -EIO;
		}

		shell_error(shell, "command failed: %d", ret);
		return ret;
	}

	ret = ncellmeas_wait(shell, ctr_lte_v2_flow_test_ncellmeas_wait);
	(void)ctr_lte_v2_flow_test_ncellmeas_end(!ret);
	if (ret) {
		return ret;
	}

	return print_ncellmeas(shell);
}

/* A state that does not take the request soon: the reason, or NULL. The request waits there (reset
 * loop, error) or is dropped (disabled: not for a waiting request, its drop ends the wait) */
static const char *scan_blocked(const char *state, bool waiting)
{
	if (!strcmp(state, "disabled")) {
		return waiting ? NULL : "LTE is disabled";
	} else if (!strcmp(state, "reset_loop")) {
		return "modem reset loop, attach blocked";
	} else if (!strcmp(state, "error")) {
		return "LTE error, retrying";
	}

	return NULL;
}

static int cmd_ncellmeas_now(const struct shell *shell, size_t argc, char **argv)
{
	if (g_ctr_lte_v2_config.test) {
		return ncellmeas_now_test(shell);
	}

	const char *state = ctr_lte_v2_get_state();
	const char *reason = scan_blocked(state, false);

	/* The retry delay takes the scan, a measurement waits for the next attach (up to hours) */
	if (!reason && !strcmp(state, "retry_delay")) {
		reason = "not registered, retrying attach";
	}

	if (reason) {
		shell_error(shell, "measurement not possible: %s (fsm-state: %s)", reason, state);
		return -EBUSY;
	}

	/* Unchanged by the end: the request was dropped */
	uint32_t ends;

	int ret = ctr_lte_v2_request_ncellmeas(&ends);
	if (ret == -EALREADY) {
		shell_print(shell, "measurement already in progress, waiting for it");
	} else if (ret == -EBUSY) {
		/* A scan requested or running: no request */
		state = ctr_lte_v2_get_state();
		shell_error(shell, "measurement not possible: scan in progress (fsm-state: %s)",
			    state);
		return ret;
	} else if (ret) {
		shell_error(shell, "command failed: %d", ret);
		return ret;
	}

	ret = ncellmeas_wait(shell, ctr_lte_v2_wait_for_ncellmeas);
	if (ret) {
		return ret;
	}

	if (ctr_lte_v2_get_ncellmeas_ends() == ends) {
		shell_error(shell, "measurement dropped");
		return -ECANCELED;
	}

	return print_ncellmeas(shell);
}

static int cmd_ncellmeas_schedule(const struct shell *shell, size_t argc, char **argv)
{
	if (g_ctr_lte_v2_config.test) {
		shell_error(shell, "not supported in test mode");
		return -ENOEXEC;
	}

	int ret = ctr_lte_v2_schedule_ncellmeas();
	if (ret == -EBUSY) {
		shell_error(shell, "scan in progress");
		return ret;
	} else if (ret && ret != -EALREADY) {
		LOG_ERR("Call `ctr_lte_v2_schedule_ncellmeas` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_info(shell, "command succeeded");

	return 0;
}

static int cmd_ncellmeas_show(const struct shell *shell, size_t argc, char **argv)
{
	return print_ncellmeas(shell);
}

static const char *cops_stat_str(int stat)
{
	switch (stat) {
	case 1:
		return "available";
	case 2:
		return "current";
	case 3:
		return "forbidden";
	default:
		return "unknown";
	}
}

static const char *cops_act_str(int act)
{
	switch (act) {
	case 7:
		return "lte-m";
	case 9:
		return "nb-iot";
	default:
		return "unknown";
	}
}

static const char *scan_status_str(int status)
{
	switch (status) {
	case 0:
		return "ok";
	case -ENODATA:
		return "not run";
	case -EINTR:
		return "partial";
	case -EBUSY:
		return "radio busy";
	case -ECANCELED:
		return "cancelled";
	default:
		return "error";
	}
}

/* The cells step stopped at its bound has the cells found so far */
static int scan_cells_status(int status)
{
	return status == -ETIMEDOUT ? -EINTR : status;
}

/* One word for the lists of the RATs that ran: all ok, some ok (partial), none ok (error: each
 * failure of the list is an error in these words) */
static int scan_networks_status(const struct ctr_lte_v2_scan_result *r)
{
	const int status[] = {r->lte_m.networks, r->nb_iot.networks};
	int run = 0;
	int ok = 0;

	for (size_t i = 0; i < ARRAY_SIZE(status); i++) {
		run += status[i] != -ENODATA;
		ok += status[i] == 0;
	}

	if (!run) {
		return -ENODATA;
	}

	if (ok == run) {
		return 0;
	}

	return ok ? -EINTR : -EIO;
}

/* MCC and MNC as one key in their order, -1 for an invalid PLMN */
static int32_t scan_plmn_key(const char *plmn)
{
	if (ctr_lte_v2_parse_plmn(plmn)) {
		return -1;
	}

	int32_t mcc = (plmn[0] - '0') * 100 + (plmn[1] - '0') * 10 + (plmn[2] - '0');

	return mcc * 1000 + strtol(plmn + 3, NULL, 10);
}

/* Stronger first within a RAT; LTE-M before NB-IoT */
static bool scan_cell_before(const struct ctr_lte_v2_scan_cell *a,
			     const struct ctr_lte_v2_scan_cell *b)
{
	return a->act != b->act ? a->act < b->act : a->rsrp > b->rsrp;
}

/* One group: the PLMN with its RATs and stat from the lists, and its cells. True if listed */
static bool print_scan_plmn(const struct shell *shell, const struct ctr_lte_v2_scan_result *r,
			    size_t entries, int32_t key)
{
	int stat_ltem = -1;
	int stat_nbiot = -1;
	bool ltem = false;
	bool nbiot = false;
	const char *plmn = NULL;

	for (size_t i = 0; i < entries; i++) {
		const struct ctr_lte_v2_scan_network *e = &r->networks[i];

		if (scan_plmn_key(e->plmn) != key) {
			continue;
		}
		if (!plmn) {
			plmn = e->plmn;
		}
		if (e->act == CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT) {
			stat_nbiot = e->stat;
			nbiot = true;
		} else {
			stat_ltem = e->stat;
			ltem = true;
		}
	}

	bool listed = plmn != NULL;
	uint8_t cells[CTR_LTE_V2_SCAN_CELL_MAX];
	size_t n = 0;

	for (size_t i = 0; i < MIN(r->cell_count, CTR_LTE_V2_SCAN_CELL_MAX); i++) {
		const struct ctr_lte_v2_scan_cell *c = &r->cells[i];

		if (scan_plmn_key(c->plmn) != key) {
			continue;
		}
		if (!plmn) {
			plmn = c->plmn;
		}
		if (c->act == CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT) {
			nbiot = true;
		} else {
			ltem = true;
		}

		size_t j = n++;
		for (; j > 0 && scan_cell_before(c, &r->cells[cells[j - 1]]); j--) {
			cells[j] = cells[j - 1];
		}
		cells[j] = i;
	}

	char stat[48];

	if (stat_ltem < 0 && stat_nbiot < 0) {
		snprintf(stat, sizeof(stat), "not in network list");
	} else if (stat_ltem < 0 || stat_nbiot < 0 || stat_ltem == stat_nbiot) {
		snprintf(stat, sizeof(stat), "%s",
			 cops_stat_str(stat_ltem >= 0 ? stat_ltem : stat_nbiot));
	} else {
		snprintf(stat, sizeof(stat), "%s (lte-m), %s (nb-iot)", cops_stat_str(stat_ltem),
			 cops_stat_str(stat_nbiot));
	}

	shell_print(shell, "plmn: %s, act: %s%s%s, stat: %s", plmn, ltem ? "lte-m" : "",
		    ltem && nbiot ? "," : "", nbiot ? "nb-iot" : "", stat);

	for (size_t i = 0; i < n; i++) {
		const struct ctr_lte_v2_scan_cell *c = &r->cells[cells[i]];

		shell_print(shell,
			    "  cell: %08X, act: %s, tac: %04X, earfcn: %u, pci: %u, rsrp: %d dBm",
			    (unsigned int)c->eci, cops_act_str(c->act), c->tac,
			    (unsigned int)c->earfcn, c->pci, ctr_lte_v2_parse_rsrp_dbm(c->rsrp));
	}

	return listed;
}

/* One group per PLMN with all its RATs and cells, in the order of MCC and MNC: each round takes
 * the smallest key above the last one (no arrays on the stack) */
static void print_scan_groups(const struct shell *shell, const struct ctr_lte_v2_scan_result *r)
{
	size_t entries = MIN(r->network_count, CTR_LTE_V2_SCAN_NETWORK_MAX);
	size_t cell_count = MIN(r->cell_count, CTR_LTE_V2_SCAN_CELL_MAX);
	size_t listed = 0;
	int32_t key = -1;

	for (;;) {
		int32_t next = INT32_MAX;

		for (size_t i = 0; i < entries + cell_count; i++) {
			int32_t k = scan_plmn_key(i < entries ? r->networks[i].plmn
							      : r->cells[i - entries].plmn);
			if (k > key && k < next) {
				next = k;
			}
		}

		if (next == INT32_MAX) {
			break;
		}

		key = next;
		listed += print_scan_plmn(shell, r, entries, key);
	}

	if (entries < r->network_count) {
		shell_print(shell, "networks: %zu (%u with act, %zu stored), cells: %u", listed,
			    r->network_count, entries, r->cell_count);
	} else {
		shell_print(shell, "networks: %zu (%u with act), cells: %u", listed,
			    r->network_count, r->cell_count);
	}
}

static int print_scan_result(const struct shell *shell)
{
	/* A copy (on the stack): printing under the lock of the state would hold up the URCs */
	struct ctr_lte_v2_scan_result result;

	int ret = ctr_lte_v2_get_scan_result(&result);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_scan_result` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	if (!result.valid) {
		shell_print(shell, "no scan yet");
		shell_info(shell, "command succeeded");
		return 0;
	}

	print_last_reject(shell);

	char ago[24];
	/* From the end, as hio */
	uint32_t age_s = (k_uptime_get() - result.uptime_ms) / MSEC_PER_SEC - result.duration_s;

	ctr_util_fmt_duration(age_s, ago, sizeof(ago));
	shell_print(shell, "last scan: %s ago (request)", ago);
	shell_print(shell, "search: lte-m cells %s, nb-iot cells %s, networks %s",
		    scan_status_str(scan_cells_status(result.lte_m.cells)),
		    scan_status_str(scan_cells_status(result.nb_iot.cells)),
		    scan_status_str(scan_networks_status(&result)));

	print_scan_groups(shell, &result);

	shell_info(shell, "command succeeded");

	return 0;
}

/* The optional mode in argv[1], the same for each command that takes it */
static int scan_mode_parse(const struct shell *shell, size_t argc, char **argv,
			   enum ctr_lte_v2_scan_mode *mode)
{
	*mode = CTR_LTE_V2_SCAN_ALL;

	if (argc > 2) {
		shell_error(shell, "only one argument is accepted");
		shell_help(shell);
		return -EINVAL;
	}

	if (argc == 2) {
		if (!strcmp(argv[1], "all")) {
			*mode = CTR_LTE_V2_SCAN_ALL;
		} else if (!strcmp(argv[1], "plmn")) {
			*mode = CTR_LTE_V2_SCAN_PLMN;
		} else if (!strcmp(argv[1], "cells")) {
			*mode = CTR_LTE_V2_SCAN_CELLS;
		} else {
			shell_error(shell, "unknown mode: %s", argv[1]);
			shell_help(shell);
			return -EINVAL;
		}
	}

	return 0;
}

static int cmd_scan_now(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	enum ctr_lte_v2_scan_mode mode;

	ret = scan_mode_parse(shell, argc, argv, &mode);
	if (ret) {
		return ret;
	}

	/* Test mode: no FSM, the steps run directly */
	bool test = g_ctr_lte_v2_config.test;
	const char *state = ctr_lte_v2_get_state();
	const char *reason = test ? NULL : scan_blocked(state, false);

	if (reason) {
		shell_error(shell, "scan not possible: %s (fsm-state: %s)", reason, state);
		return -EBUSY;
	}

	if (test && ctr_lte_v2_flow_test_link_up()) {
		shell_error(shell, "scan not possible: modem not ready");
		return -EIO;
	}

	/* Unchanged by the end: the request was dropped */
	uint32_t ends;

	ret = test ? ctr_lte_v2_test_scan(mode, &ends) : ctr_lte_v2_request_scan(mode, &ends);
	if (ret == -EALREADY) {
		shell_print(shell, "scan already in progress, waiting for it");
	} else if (ret) {
		shell_error(shell, "command failed: %d", ret);
		return ret;
	}

	/* Cannot be interrupted anyway, so hold the shell until it is done. The connection is
	 * re-established after the scan. */
	shell_print(shell, "scanning, this takes minutes...");

	int64_t command = k_uptime_get();
	int64_t start = 0;
	int64_t last;
	k_timeout_t wait;

	for (;;) {
		if (!start) {
			ctr_lte_v2_state_get_scan_starts(&start, &last);
		}

		if (start) {
			/* A line each 30 s from the start */
			wait = K_MSEC(30000 - (k_uptime_get() - start) % 30000);
		} else {
			/* Not started: a state that blocks it ends the wait, the request stays */
			state = ctr_lte_v2_get_state();
			reason = test ? NULL : scan_blocked(state, true);
			if (reason) {
				shell_error(shell, "scan not possible: %s (fsm-state: %s)", reason,
					    state);
				return -EBUSY;
			}

			wait = K_SECONDS(1);
		}

		if (!ctr_lte_v2_wait_for_scan(wait)) {
			break;
		}

		/* Beyond the bounds of the steps: something is wrong */
		int64_t elapsed = (k_uptime_get() - (start ? start : command)) / MSEC_PER_SEC;

		if (elapsed > 35 * 60) {
			shell_error(shell, "scan did not finish");
			return -ETIMEDOUT;
		}

		if (start) {
			char took[24];

			ctr_util_fmt_duration(elapsed, took, sizeof(took));
			shell_print(shell, "still scanning (%s)", took);
		}
	}

	/* Dropped (a reconnect, LTE disabled): no new result */
	if (ctr_lte_v2_get_scan_ends() == ends) {
		shell_error(shell, "scan dropped");
		return -ECANCELED;
	}

	return print_scan_result(shell);
}

static int cmd_scan_schedule(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	enum ctr_lte_v2_scan_mode mode;

	ret = scan_mode_parse(shell, argc, argv, &mode);
	if (ret) {
		return ret;
	}

	if (g_ctr_lte_v2_config.test) {
		shell_error(shell, "not supported in test mode");
		return -ENOEXEC;
	}

	ret = ctr_lte_v2_scan(mode);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `ctr_lte_v2_scan` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_info(shell, "command succeeded");

	return 0;
}

static int cmd_scan_show(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	return print_scan_result(shell);
}
#endif

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
	              "Enable/disable UART interface <enable|disable>.",
	              cmd_test_uart, 2, 0),

	SHELL_CMD_ARG(reset, NULL,
	              "Reset modem.",
	              cmd_test_reset, 1, 0),

	SHELL_CMD_ARG(wakeup, NULL,
	              "Wake up modem.",
	              cmd_test_wakeup, 1, 0),

	SHELL_CMD_ARG(cmd, NULL,
	              "Send command to modem <command>.",
	              cmd_test_cmd, 2, 0),

	SHELL_CMD_ARG(prepare, NULL, "Run prepare modem sequence.", cmd_test_prepare, 1, 0),

	SHELL_CMD_ARG(bypass, NULL, "Switch to bypass mode.", cmd_test_bypass, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_DYNAMIC_CMD_CREATE(sub_lte_config, ctr_lte_v2_config_subcmd_get);

#if defined(CONFIG_CTR_LTE_V2_SCAN)
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte_ncellmeas,

	SHELL_CMD_ARG(now, NULL,
	              "Measure and wait for the result.",
	              cmd_ncellmeas_now, 1, 0),

	SHELL_CMD_ARG(schedule, NULL,
	              "Request a measurement, see 'show'.",
	              cmd_ncellmeas_schedule, 1, 0),

	SHELL_CMD_ARG(show, NULL,
	              "Show the last measurement.",
	              cmd_ncellmeas_show, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte_scan,

	SHELL_CMD_ARG(now, NULL,
	              "Scan and wait for the result (format: [all|plmn|cells]).",
	              cmd_scan_now, 1, 1),

	SHELL_CMD_ARG(schedule, NULL,
	              "Request a scan, see 'show' (format: [all|plmn|cells]).",
	              cmd_scan_schedule, 1, 1),

	SHELL_CMD_ARG(show, NULL,
	              "Show the last scan result.",
	              cmd_scan_show, 1, 0),

	SHELL_SUBCMD_SET_END
);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte,

	SHELL_CMD_ARG(config, &sub_lte_config,
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

	SHELL_CMD_ARG(history, NULL,
	              "Show recent registration changes (CEREG, oldest first) and the "
	              "last reject.",
	              cmd_history, 1, 0),

#if defined(CONFIG_CTR_LTE_V2_SCAN)
	SHELL_CMD(ncellmeas, &sub_lte_ncellmeas,
	          "Measure the neighbour cells.",
	          print_help),

	SHELL_CMD(scan, &sub_lte_scan,
	          "Scan cells and networks of the configured RATs; interrupts "
	          "attach and reconnects. Leaves the modem in CFUN=1 in test mode.",
	          print_help),
#endif

	SHELL_CMD_ARG(test, &sub_lte_test,
	              "Test commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);
