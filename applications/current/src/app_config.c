#include "app_config.h"
#include <ctr_config.h>

/* Zephyr includes */
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;
static struct app_config m_app_config_interim = {
	.current_range = 10,
	.report_interval = 900,
	.scan_interval = 60,
};

static void print_current_range(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config current-range %d",
	            m_app_config_interim.current_range);
}

static void print_report_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config report-interval %d",
	            m_app_config_interim.report_interval);
}

static void print_scan_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config scan-interval %d",
	            m_app_config_interim.scan_interval);
}

static void print_sensor_test(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config sensor-test %s",
	            m_app_config_interim.sensor_test ? "true" : "false");
}

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_current_range(shell);
	print_report_interval(shell);
	print_scan_interval(shell);
	print_sensor_test(shell);

	return 0;
}

int app_config_cmd_config_current_range(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_current_range(shell);
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

		int current_range = atoi(argv[1]);

		if (current_range < 1 || current_range > 10000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.current_range = current_range;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_report_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_report_interval(shell);
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

		int report_interval = atoi(argv[1]);

		if (report_interval < 30 || report_interval > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.report_interval = report_interval;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_scan_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_scan_interval(shell);
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

		int scan_interval = atoi(argv[1]);

		if (scan_interval < 5 || scan_interval > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.scan_interval = scan_interval;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_sensor_test(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_sensor_test(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_app_config_interim.sensor_test = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_app_config_interim.sensor_test = false;
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

	SETTINGS_SET("current-range", &m_app_config_interim.current_range,
	             sizeof(m_app_config_interim.current_range));
	SETTINGS_SET("report-interval", &m_app_config_interim.report_interval,
	             sizeof(m_app_config_interim.report_interval));
	SETTINGS_SET("scan-interval", &m_app_config_interim.scan_interval,
	             sizeof(m_app_config_interim.scan_interval));
	SETTINGS_SET("sensor-test", &m_app_config_interim.sensor_test,
	             sizeof(m_app_config_interim.sensor_test));

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

	EXPORT_FUNC("current-range", &m_app_config_interim.current_range,
	            sizeof(m_app_config_interim.current_range));
	EXPORT_FUNC("report-interval", &m_app_config_interim.report_interval,
	            sizeof(m_app_config_interim.report_interval));
	EXPORT_FUNC("scan-interval", &m_app_config_interim.scan_interval,
	            sizeof(m_app_config_interim.scan_interval));
	EXPORT_FUNC("sensor-test", &m_app_config_interim.sensor_test,
	            sizeof(m_app_config_interim.sensor_test));

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

	if (ret < 0) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);

	if (ret < 0) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
