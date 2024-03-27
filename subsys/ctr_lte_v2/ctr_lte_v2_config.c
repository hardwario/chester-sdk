/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2_config, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define SETTINGS_PFX "lte"

struct ctr_lte_v2_config g_ctr_lte_v2_config;

struct ctr_lte_v2_config m_config = {
	.antenna = CTR_LTE_V2_CONFIG_ANTENNA_INT,
	.nb_iot_mode = true,
#if defined(CONFIG_CTR_LTE_V2_CLKSYNC)
	.clksync = true,
#endif
	.plmnid = "23003",
	.apn = "hardwario",
	.auth = CTR_LTE_V2_CONFIG_AUTH_NONE,
	.addr = {192, 168, 192, 4},
	.port = 5002,
};

static void print_test(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config test %s", m_config.test ? "true" : "false");
}

static void print_modemtrace(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config modemtrace %s",
		    m_config.modemtrace ? "true" : "false");
}

static void print_antenna(const struct shell *shell)
{
	switch (m_config.antenna) {
	case CTR_LTE_V2_CONFIG_ANTENNA_INT:
		shell_print(shell, SETTINGS_PFX " config antenna int");
		break;
	case CTR_LTE_V2_CONFIG_ANTENNA_EXT:
		shell_print(shell, SETTINGS_PFX " config antenna ext");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config antenna (unknown)");
	}
}

static void print_nb_iot_mode(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config nb-iot-mode %s",
		    m_config.nb_iot_mode ? "true" : "false");
}

static void print_lte_m_mode(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config lte-m-mode %s",
		    m_config.lte_m_mode ? "true" : "false");
}

static void print_autoconn(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config autoconn %s",
		    m_config.autoconn ? "true" : "false");
}

static void print_plmnid(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config plmnid %s", m_config.plmnid);
}

static void print_clksync(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config clksync %s", m_config.clksync ? "true" : "false");
}

static void print_apn(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config apn %s", m_config.apn);
}

static void print_auth(const struct shell *shell)
{
	switch (m_config.auth) {
	case CTR_LTE_V2_CONFIG_AUTH_NONE:
		shell_print(shell, SETTINGS_PFX " config auth none");
		break;
	case CTR_LTE_V2_CONFIG_AUTH_CHAP:
		shell_print(shell, SETTINGS_PFX " config auth chap");
		break;
	case CTR_LTE_V2_CONFIG_AUTH_PAP:
		shell_print(shell, SETTINGS_PFX " config auth pap");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config auth (unknown)");
	}
}

static void print_username(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config username %s", m_config.username);
}

static void print_password(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config password %s", m_config.password);
}

static void print_addr(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config addr %u.%u.%u.%u", m_config.addr[0],
		    m_config.addr[1], m_config.addr[2], m_config.addr[3]);
}

static void print_port(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config port %d", m_config.port);
}

int ctr_lte_v2_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	print_test(shell);
	print_modemtrace(shell);
	print_antenna(shell);
	print_nb_iot_mode(shell);
	print_lte_m_mode(shell);
	print_autoconn(shell);
	print_plmnid(shell);
	print_clksync(shell);
	print_apn(shell);
	print_auth(shell);
	print_username(shell);
	print_password(shell);
	print_addr(shell);
	print_port(shell);

	return 0;
}

int ctr_lte_v2_config_cmd_test(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_test(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.test = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.test = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_modemtrace(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_modemtrace(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.modemtrace = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.modemtrace = false;
		return 0;
	}

	print_modemtrace(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_antenna(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_antenna(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "int") == 0) {
		m_config.antenna = CTR_LTE_V2_CONFIG_ANTENNA_INT;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "ext") == 0) {
		m_config.antenna = CTR_LTE_V2_CONFIG_ANTENNA_EXT;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_nb_iot_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_nb_iot_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.nb_iot_mode = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.nb_iot_mode = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_lte_m_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_lte_m_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.lte_m_mode = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.lte_m_mode = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_autoconn(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_autoconn(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.autoconn = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.autoconn = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_plmnid(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_plmnid(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len != 5 && len != 6) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config.plmnid, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_clksync(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_clksync(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.clksync = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.clksync = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_apn(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_apn(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config.apn)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			char c = argv[1][i];
			if (!isalnum((int)c) && c != '-' && c != '.') {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config.apn, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_auth(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_auth(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "none") == 0) {
		m_config.auth = CTR_LTE_V2_CONFIG_AUTH_NONE;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "pap") == 0) {
		m_config.auth = CTR_LTE_V2_CONFIG_AUTH_PAP;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "chap") == 0) {
		m_config.auth = CTR_LTE_V2_CONFIG_AUTH_CHAP;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_username(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_username(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config.username)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		strcpy(m_config.username, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_password(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_password(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config.password)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		strcpy(m_config.password, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_addr(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_addr(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len > 15) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		unsigned int addr[4];

		ret = sscanf(argv[1], "%u.%u.%u.%u", &addr[0], &addr[1], &addr[2], &addr[3]);
		if (ret != 4) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		if ((addr[0] < 0 || addr[0] > 255) || (addr[1] < 0 || addr[1] > 255) ||
		    (addr[2] < 0 || addr[2] > 255) || (addr[3] < 0 || addr[3] > 255)) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_config.addr[0] = addr[0];
		m_config.addr[1] = addr[1];
		m_config.addr[2] = addr[2];
		m_config.addr[3] = addr[3];

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_v2_config_cmd_port(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_port(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 5) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int port = strtol(argv[1], NULL, 10);

		if (port < 0 || port > 65535) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_config.port = port;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET_SCALAR(_key, _var)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != sizeof(_var)) {                                                 \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, &_var, len);                                         \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

#define SETTINGS_SET_ARRAY(_key, _var)                                                             \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != sizeof(_var)) {                                                 \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, _var, len);                                          \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET_SCALAR("test", m_config.test);
	SETTINGS_SET_SCALAR("modemtrace", m_config.modemtrace);
	SETTINGS_SET_SCALAR("antenna", m_config.antenna);
	SETTINGS_SET_SCALAR("nb-iot-mode", m_config.nb_iot_mode);
	SETTINGS_SET_SCALAR("lte-m-mode", m_config.lte_m_mode);
	SETTINGS_SET_SCALAR("autoconn", m_config.autoconn);
	SETTINGS_SET_ARRAY("plmnid", m_config.plmnid);
	SETTINGS_SET_SCALAR("clksync", m_config.clksync);
	SETTINGS_SET_ARRAY("apn", m_config.apn);
	SETTINGS_SET_SCALAR("auth", m_config.auth);
	SETTINGS_SET_ARRAY("username", m_config.username);
	SETTINGS_SET_ARRAY("password", m_config.password);
	SETTINGS_SET_ARRAY("addr", m_config.addr);
	SETTINGS_SET_SCALAR("port", m_config.port);

#undef SETTINGS_SET_SCALAR
#undef SETTINGS_SET_ARRAY

	return -ENOENT;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	memcpy(&g_ctr_lte_v2_config, &m_config, sizeof(g_ctr_lte_v2_config));

	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
#define EXPORT_FUNC_SCALAR(_key, _var)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, &_var, sizeof(_var));                     \
	} while (0)

#define EXPORT_FUNC_ARRAY(_key, _var)                                                              \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, sizeof(_var));                      \
	} while (0)

	EXPORT_FUNC_SCALAR("test", m_config.test);
	EXPORT_FUNC_SCALAR("modemtrace", m_config.modemtrace);
	EXPORT_FUNC_SCALAR("antenna", m_config.antenna);
	EXPORT_FUNC_SCALAR("nb-iot-mode", m_config.nb_iot_mode);
	EXPORT_FUNC_SCALAR("lte-m-mode", m_config.lte_m_mode);
	EXPORT_FUNC_SCALAR("autoconn", m_config.autoconn);
	EXPORT_FUNC_ARRAY("plmnid", m_config.plmnid);
	EXPORT_FUNC_SCALAR("clksync", m_config.clksync);
	EXPORT_FUNC_ARRAY("apn", m_config.apn);
	EXPORT_FUNC_SCALAR("auth", m_config.auth);
	EXPORT_FUNC_ARRAY("username", m_config.username);
	EXPORT_FUNC_ARRAY("password", m_config.password);
	EXPORT_FUNC_ARRAY("addr", m_config.addr);
	EXPORT_FUNC_SCALAR("port", m_config.port);

#undef EXPORT_FUNC_SCALAR
#undef EXPORT_FUNC_ARRAY

	return 0;
}

static int init()
{
	int ret;

	LOG_INF("System initialization");

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, ctr_lte_v2_config_cmd_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_LTE_V2_CONFIG_INIT_PRIORITY);
