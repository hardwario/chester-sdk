/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lrw_v2_config.h"
#include "ctr_lrw_v2_flow.h"

#include <chester/ctr_config.h>
#include <chester/ctr_lrw_v2.h>
#include <chester/ctr_util.h>
#include <chester/drivers/ctr_lrw_link.h>
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lrw_v2, CONFIG_CTR_LRW_V2_LOG_LEVEL);

#define SETTINGS_PFX "lrw_v2"

#define BOOT_RETRY_COUNT     3
#define BOOT_RETRY_DELAY     K_SECONDS(10)
#define SETUP_RETRY_COUNT    3
#define SETUP_RETRY_DELAY    K_SECONDS(10)
#define JOIN_TIMEOUT         K_SECONDS(120)
#define JOIN_RETRY_COUNT     3
#define JOIN_RETRY_DELAY     K_SECONDS(30)
#define SEND_TIMEOUT         K_SECONDS(120)
#define SEND_RETRY_COUNT     3
#define SEND_RETRY_DELAY     K_SECONDS(30)
#define REQ_INTRA_DELAY_MSEC 8000

#define CMD_MSGQ_MAX_ITEMS  16
#define SEND_MSGQ_MAX_ITEMS 16

int ctr_lrw_v2_init(ctr_lrw_v2_recv_cb recv_cb)
{
	int ret;

	ret = ctr_lrw_v2_flow_set_recv_cb(recv_cb);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_set_recv_cb` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_v2_start(void)
{
	int ret;

	ret = ctr_lrw_v2_flow_setup();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_setup` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_v2_join(void)
{
	int ret;

	if (g_ctr_lrw_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	ret = ctr_lrw_v2_flow_join(JOIN_RETRY_COUNT, JOIN_RETRY_DELAY);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_join` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_v2_send(const struct ctr_lrw_send_opts *opts, const void *buf, size_t len)
{
	int ret;

	if (g_ctr_lrw_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	struct send_msgq_data msg = {
		.ttl = opts->ttl,
		.confirmed = opts->confirmed,
		.port = opts->port,
		.buf = (void *)buf,
		.len = len,
	};

	ret = ctr_lrw_v2_flow_send(SEND_RETRY_COUNT, SEND_RETRY_DELAY, &msg);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_v2_poll(void)
{
	int ret;

	if (g_ctr_lrw_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	ret = ctr_lrw_v2_flow_poll();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_start(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "no arguments are accepted");
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lrw_v2_flow_setup();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_setup` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_join(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "no arguments are accepted");
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lrw_v2_flow_join(JOIN_RETRY_COUNT, JOIN_RETRY_DELAY);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_join` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_poll(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "No arguments are accepted");
		shell_help(shell);
		return -EINVAL;
	}

	ret = ctr_lrw_v2_flow_poll();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_flow_poll` failed: %d", ret);
		shell_error(shell, "Command failed");
		return ret;
	}

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
	sub_lrw_config,

	SHELL_CMD_ARG(show, NULL,
	              "List current configuration.",
	              ctr_lrw_v2_config_cmd_show, 1, 0),

	SHELL_CMD_ARG(test, NULL,
	              "Get/Set LoRaWAN test mode.",
	              ctr_lrw_v2_config_cmd_test, 1, 1),

	SHELL_CMD_ARG(antenna, NULL,
	              "Get/Set LoRaWAN antenna (format: <int|ext>).",
		      ctr_lrw_v2_config_cmd_antenna, 1, 1),

	SHELL_CMD_ARG(band, NULL,
		      "Get/Set radio band"
		      " (format: <as923|au915|eu868|kr920|in865|us915>).",
		      ctr_lrw_v2_config_cmd_band, 1, 1),

	SHELL_CMD_ARG(chmask, NULL,
	              "Get/Set channel mask (format: <N hexadecimal digits>). ",
		      ctr_lrw_v2_config_cmd_chmask, 1, 1),

	SHELL_CMD_ARG(class, NULL,
	              "Get/Set device class (format: <a|c>).",
		      ctr_lrw_v2_config_cmd_class, 1, 1),

	SHELL_CMD_ARG(mode, NULL,
	              "Get/Set operation mode (format: <abp|otaa>).",
		      ctr_lrw_v2_config_cmd_mode, 1, 1),

	SHELL_CMD_ARG(nwk, NULL,
	              "Get/Set network type (format: <private|public>).",
		      ctr_lrw_v2_config_cmd_nwk, 1, 1),

	SHELL_CMD_ARG(adr, NULL,
	              "Get/Set adaptive data rate (format: <true|false>).",
		      ctr_lrw_v2_config_cmd_adr, 1, 1),

	SHELL_CMD_ARG(datarate, NULL,
	              "Get/Set data rate (format: <number 1-15>).",
		      ctr_lrw_v2_config_cmd_datarate, 1, 1),

	SHELL_CMD_ARG(dutycycle, NULL,
	              "Get/Set duty cycle (format: <true|false>).",
		      ctr_lrw_v2_config_cmd_dutycycle, 1, 1),

	SHELL_CMD_ARG(devaddr, NULL,
	              "Get/Set DevAddr (format: <8 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_devaddr, 1, 1),

	SHELL_CMD_ARG(deveui, NULL,
	              "Get/Set DevEUI (format: <16 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_deveui, 1, 1),

	SHELL_CMD_ARG(joineui, NULL,
	              "Get/Set JoinEUI (format: <16 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_joineui, 1, 1),

	SHELL_CMD_ARG(appkey, NULL,
	              "Get/Set AppKey (format: <32 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_appkey, 1, 1),

	SHELL_CMD_ARG(nwkskey, NULL,
	              "Get/Set NwkSKey (format: <32 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_nwkskey, 1, 1),

	SHELL_CMD_ARG(appskey, NULL,
	              "Get/Set AppSKey (format: <32 hexadecimal digits>).",
		      ctr_lrw_v2_config_cmd_appskey, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lrw_test,

	SHELL_CMD_ARG(uart, NULL,
	              "Enable/Disable UART interface (format: <enable|disable>).",
		      ctr_lrw_v2_flow_cmd_test_uart, 2, 0),
	SHELL_CMD_ARG(reset, NULL,
	              "Reset modem.",
		      ctr_lrw_v2_flow_cmd_test_reset, 1, 0),

	SHELL_CMD_ARG(cmd, NULL,
	              "Send command to modem. (format: <command>).",
		      ctr_lrw_v2_flow_cmd_test_cmd, 2, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lrw,

	SHELL_CMD_ARG(config, &sub_lrw_config,
	              "Configuration commands.",
		      print_help, 1, 0),

	SHELL_CMD_ARG(start, &sub_lrw_test,
	              "Start interface.",
		      cmd_start, 1, 0),

	SHELL_CMD_ARG(join, NULL,
	              "Join network.",
		      cmd_join, 1, 0),

	SHELL_CMD_ARG(poll, NULL,
	              "Poll for interface asynchronous events.",
		      cmd_poll, 1, 0),

	SHELL_CMD_ARG(test, &sub_lrw_test,
	              "Test commands.",
		      print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lrw, &sub_lrw, "LoRaWAN commands.", print_help);

/* clang-format on */

BUILD_ASSERT(CONFIG_CTR_LRW_V2_INIT_PRIORITY > CONFIG_CTR_LRW_V2_CONFIG_INIT_PRIORITY,
	     "CONFIG_CTR_LRW_V2_INIT_PRIORITY must be higher than"
	     "CONFIG_CTR_LRW_V2_CONFIG_PRIORITY");
