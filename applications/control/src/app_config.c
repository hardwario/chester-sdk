/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-input"

struct app_config g_app_config;

static struct app_config m_app_config_interim = {
	.interval_report = 1800,
	.interval_poll = 60,

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	.event_report_delay = 5,
	.event_report_rate = 30,
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	.backup_report_connected = true,
	.backup_report_disconnected = true,
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	.channel_mode_1 = APP_CONFIG_CHANNEL_MODE_TRIGGER,
	.channel_mode_2 = APP_CONFIG_CHANNEL_MODE_COUNTER,
	.channel_mode_3 = APP_CONFIG_CHANNEL_MODE_VOLTAGE,
	.channel_mode_4 = APP_CONFIG_CHANNEL_MODE_CURRENT,

	.trigger_input_type = APP_CONFIG_INPUT_TYPE_NPN,
	.trigger_duration_active = 100,
	.trigger_duration_inactive = 100,
	.trigger_cooldown_time = 10,

	.counter_interval_aggreg = 300,
	.counter_input_type = APP_CONFIG_INPUT_TYPE_NPN,
	.counter_duration_active = 2,
	.counter_duration_inactive = 2,
	.counter_cooldown_time = 10,
	.analog_interval_sample = 60,
	.analog_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	.hygro_interval_sample = 60,
	.hygro_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	.w1_therm_interval_sample = 60,
	.w1_therm_interval_aggreg = 300,
#endif /* defined(COINFG_SHIELD_CTR_DS18B20)*/
};

char *app_config_enum_trigger_input_type[] = {"npn", "pnp"};
char *app_config_enum_channel_mode[] = {"trigger", "counter", "voltage", "current"};

#define DEFINE_CMD_CONFIG_INT(_name_d, _name_u, _min, _max)                                        \
	static void print_##_name_u(const struct shell *shell)                                     \
	{                                                                                          \
		shell_print(shell, "app config " #_name_d " %d", m_app_config_interim._name_u);    \
	}                                                                                          \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv)   \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##_name_u(shell);                                                    \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			size_t len = strlen(argv[1]);                                              \
			for (size_t i = 0; i < len; i++) {                                         \
				if (!isdigit((int)argv[1][i])) {                                   \
					shell_error(shell, "invalid format");                      \
					return -EINVAL;                                            \
				}                                                                  \
			}                                                                          \
			long value = strtol(argv[1], NULL, 10);                                    \
			if (value < _min || value > _max) {                                        \
				shell_error(shell, "invalid range");                               \
				return -EINVAL;                                                    \
			}                                                                          \
			m_app_config_interim._name_u = (int)value;                                 \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

#define DEFINE_CMD_CONFIG_FLOAT(_name_d, _name_u, _min, _max)                                      \
	static void print_##_name_u(const struct shell *shell)                                     \
	{                                                                                          \
		shell_print(shell, "app config " #_name_d " %.1f", m_app_config_interim._name_u);  \
	}                                                                                          \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv)   \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##_name_u(shell);                                                    \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			float value;                                                               \
			int ret = sscanf(argv[1], "%f", &value);                                   \
			if (ret != 1) {                                                            \
				shell_error(shell, "invalid value");                               \
				return -EINVAL;                                                    \
			}                                                                          \
			if (value < _min || value > _max) {                                        \
				shell_error(shell, "invalid range");                               \
				return -EINVAL;                                                    \
			}                                                                          \
			m_app_config_interim._name_u = value;                                      \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

#define DEFINE_CMD_CONFIG_BOOL(_name_d, _name_u)                                                   \
	static void print_##_name_u(const struct shell *shell)                                     \
	{                                                                                          \
		shell_print(shell, "app config " #_name_d " %s",                                   \
			    m_app_config_interim._name_u ? "true" : "false");                      \
	}                                                                                          \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv)   \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##_name_u(shell);                                                    \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			bool is_false = !strcmp(argv[1], "false");                                 \
			bool is_true = !strcmp(argv[1], "true");                                   \
			if (is_false) {                                                            \
				m_app_config_interim._name_u = false;                              \
			} else if (is_true) {                                                      \
				m_app_config_interim._name_u = true;                               \
			} else {                                                                   \
				shell_error(shell, "invalid format");                              \
				return -EINVAL;                                                    \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

#define DEFINE_CMD_CONFIG_ENUM(_name_d, _name_u, _items, _count)                                   \
	static void print_##_name_u(const struct shell *shell)                                     \
	{                                                                                          \
		shell_print(shell, "app config " #_name_d " %s",                                   \
			    _items[m_app_config_interim._name_u]);                                 \
	}                                                                                          \
	int app_config_cmd_config_##_name_u(const struct shell *shell, size_t argc, char **argv)   \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##_name_u(shell);                                                    \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			int value = -1;                                                            \
			for (size_t i = 0; i < _count; i++) {                                      \
				if (strcmp(argv[1], _items[i]) == 0) {                             \
					value = i;                                                 \
					break;                                                     \
				}                                                                  \
			}                                                                          \
			if (value < 0) {                                                           \
				shell_error(shell, "invalid range");                               \
				return -EINVAL;                                                    \
			}                                                                          \
			m_app_config_interim._name_u = value;                                      \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help)                                      \
	DEFINE_CMD_CONFIG_INT(_name_d, _name_u, _min, _max)

#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	DEFINE_CMD_CONFIG_FLOAT(_name_d, _name_u, _min, _max)

#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help) DEFINE_CMD_CONFIG_BOOL(_name_d, _name_u)

#define CONFIG_PARAM_ENUM(_name_d, _name_u, _items, _count, _help)                                 \
	DEFINE_CMD_CONFIG_ENUM(_name_d, _name_u, _items, _count)

CONFIG_PARAM_LIST()

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL
#undef CONFIG_PARAM_ENUM

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help)      print_##_name_u(shell);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)    print_##_name_u(shell);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help)                 print_##_name_u(shell);
#define CONFIG_PARAM_ENUM(_name_d, _name_u, _items, _count, _help) print_##_name_u(shell);

	CONFIG_PARAM_LIST()

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL
#undef CONFIG_PARAM_ENUM

	return 0;
}
static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET_ARRAY(_key, _var, _size)                                                      \
	do {                                                                                       \
		if (settings_name_steq(key, #_key, &next) && !next) {                              \
			if (len != _size) {                                                        \
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

#define SETTINGS_SET_SCALAR(_key, _var)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, #_key, &next) && !next) {                              \
			if (len != sizeof(m_app_config_interim._var)) {                            \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, &m_app_config_interim._var, len);                    \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help) SETTINGS_SET_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	SETTINGS_SET_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help) SETTINGS_SET_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_ENUM(_name_d, _name_u, _items, _count, _help)                                 \
	SETTINGS_SET_SCALAR(_name_d, _name_u);

	CONFIG_PARAM_LIST()

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL
#undef CONFIG_PARAM_ENUM

#undef SETTINGS_SET_ARRAY
#undef SETTINGS_SET_SCALAR

	return -ENOENT;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_app_config, &m_app_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{

#define EXPORT_FUNC_ARRAY(_key, _var, _size)                                                       \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" #_key, _var, _size);                            \
	} while (0)

#define EXPORT_FUNC_SCALAR(_key, _var)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" #_key, &m_app_config_interim._var,              \
				  sizeof(m_app_config_interim._var));                              \
	} while (0)

#define CONFIG_PARAM_INT(_name_d, _name_u, _min, _max, _help) EXPORT_FUNC_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_FLOAT(_name_d, _name_u, _min, _max, _help)                                    \
	EXPORT_FUNC_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_BOOL(_name_d, _name_u, _help) EXPORT_FUNC_SCALAR(_name_d, _name_u);
#define CONFIG_PARAM_ENUM(_name_d, _name_u, _items, _count, _help)                                 \
	EXPORT_FUNC_SCALAR(_name_d, _name_u);

	CONFIG_PARAM_LIST()

#undef CONFIG_PARAM_INT
#undef CONFIG_PARAM_FLOAT
#undef CONFIG_PARAM_BOOL
#undef CONFIG_PARAM_ENUM

#undef EXPORT_FUNC_ARRAY
#undef EXPORT_FUNC_SCALAR

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

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
