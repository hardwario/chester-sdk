#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;

static struct app_config m_app_config_interim = {
        .interval_sample = 60,
        .interval_aggregate = 300,
        .interval_report = 1800,
};

static void print_interval_sample(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config interval-sample %d",
	            m_app_config_interim.interval_sample);
}

static void print_interval_aggregate(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config interval-aggregate %d",
	            m_app_config_interim.interval_aggregate);
}

static void print_interval_report(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config interval-report %d",
	            m_app_config_interim.interval_report);
}

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_interval_sample(shell);
	print_interval_aggregate(shell);
	print_interval_report(shell);

	return 0;
}

int app_config_cmd_config_interval_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_interval_sample(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 4) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		long interval_sample = strtol(argv[1], NULL, 10);

		if (interval_sample < 1 || interval_sample > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_sample = (int)interval_sample;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_interval_aggregate(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_interval_aggregate(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 4) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		long interval_aggregate = strtol(argv[1], NULL, 10);

		if (interval_aggregate < 1 || interval_aggregate > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_aggregate = (int)interval_aggregate;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_interval_report(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 4) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		long interval_report = strtol(argv[1], NULL, 10);

		if (interval_report < 30 || interval_report > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_report = (int)interval_report;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET(_key, _var, _size)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
                                                                                                   \
			ret = read_cb(cb_arg, _var, len);                                          \
                                                                                                   \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
                                                                                                   \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET("interval-sample", &m_app_config_interim.interval_sample,
	             sizeof(m_app_config_interim.interval_sample));
	SETTINGS_SET("interval-aggregate", &m_app_config_interim.interval_aggregate,
	             sizeof(m_app_config_interim.interval_aggregate));
	SETTINGS_SET("interval-report", &m_app_config_interim.interval_report,
	             sizeof(m_app_config_interim.interval_report));

#undef SETTINGS_SET

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
#define EXPORT_FUNC(_key, _var, _size)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

	EXPORT_FUNC("interval-sample", &m_app_config_interim.interval_sample,
	            sizeof(m_app_config_interim.interval_sample));
	EXPORT_FUNC("interval-aggregate", &m_app_config_interim.interval_aggregate,
	            sizeof(m_app_config_interim.interval_aggregate));
	EXPORT_FUNC("interval-report", &m_app_config_interim.interval_report,
	            sizeof(m_app_config_interim.interval_report));

#undef EXPORT_FUNC

	return 0;
}

static int init(const struct device *dev)
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
