#include "app_config.h"
#include <ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;
static struct app_config m_app_config_interim = {
	.measurement_interval = 60,
	.report_interval = 1800,
};

static void print_channel_active(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-active %d %s", i + 1,
		            m_app_config_interim.channel_active[i] ? "true" : "false");
	}
}

static void print_channel_differential(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-differential %d %s", i + 1,
		            m_app_config_interim.channel_differential[i] ? "true" : "false");
	}
}

static void print_channel_calib_x0(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-calib-x0 %d %d", i + 1,
		            m_app_config_interim.channel_calib_x0[i]);
	}
}

static void print_channel_calib_y0(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-calib-y0 %d %d", i + 1,
		            m_app_config_interim.channel_calib_y0[i]);
	}
}

static void print_channel_calib_x1(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-calib-x1 %d %d", i + 1,
		            m_app_config_interim.channel_calib_x1[i]);
	}
}

static void print_channel_calib_y1(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, SETTINGS_PFX " config channel-calib-y1 %d %d", i + 1,
		            m_app_config_interim.channel_calib_y1[i]);
	}
}

static void print_measurement_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config measurement-interval %d",
	            m_app_config_interim.measurement_interval);
}

static void print_report_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config report-interval %d",
	            m_app_config_interim.report_interval);
}

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_channel_active(shell, 0);
	print_channel_differential(shell, 0);
	print_channel_calib_x0(shell, 0);
	print_channel_calib_y0(shell, 0);
	print_channel_calib_x1(shell, 0);
	print_channel_calib_y1(shell, 0);
	print_measurement_interval(shell);
	print_report_interval(shell);

	return 0;
}

int app_config_cmd_config_channel_active(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_active(shell, channel);
		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "true") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_active[i] = true;
		}

		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "false") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_active[i] = false;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_differential(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_differential(shell, channel);
		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "true") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_differential[i] = true;
		}

		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "false") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_differential[i] = false;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_x0(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_x0(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_x0[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_y0(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_y0(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_y0[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_x1(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_x1(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_x1[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_y1(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_y1(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_y1[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_measurement_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_measurement_interval(shell);
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

		int measurement_interval = atoi(argv[1]);

		if (measurement_interval < 5 || measurement_interval > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.measurement_interval = measurement_interval;

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

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-active", &m_app_config_interim.channel_active[0],
	             sizeof(m_app_config_interim.channel_active[0]));
	SETTINGS_SET("channel-2-active", &m_app_config_interim.channel_active[1],
	             sizeof(m_app_config_interim.channel_active[1]));
	SETTINGS_SET("channel-3-active", &m_app_config_interim.channel_active[2],
	             sizeof(m_app_config_interim.channel_active[2]));
	SETTINGS_SET("channel-4-active", &m_app_config_interim.channel_active[3],
	             sizeof(m_app_config_interim.channel_active[3]));

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-differential", &m_app_config_interim.channel_differential[0],
	             sizeof(m_app_config_interim.channel_differential[0]));
	SETTINGS_SET("channel-2-differential", &m_app_config_interim.channel_differential[1],
	             sizeof(m_app_config_interim.channel_differential[1]));
	SETTINGS_SET("channel-3-differential", &m_app_config_interim.channel_differential[2],
	             sizeof(m_app_config_interim.channel_differential[2]));
	SETTINGS_SET("channel-4-differential", &m_app_config_interim.channel_differential[3],
	             sizeof(m_app_config_interim.channel_differential[3]));

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-calib-x0", &m_app_config_interim.channel_calib_x0[0],
	             sizeof(m_app_config_interim.channel_calib_x0[0]));
	SETTINGS_SET("channel-2-calib-x0", &m_app_config_interim.channel_calib_x0[1],
	             sizeof(m_app_config_interim.channel_calib_x0[1]));
	SETTINGS_SET("channel-3-calib-x0", &m_app_config_interim.channel_calib_x0[2],
	             sizeof(m_app_config_interim.channel_calib_x0[2]));
	SETTINGS_SET("channel-4-calib-x0", &m_app_config_interim.channel_calib_x0[3],
	             sizeof(m_app_config_interim.channel_calib_x0[3]));

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-calib-y0", &m_app_config_interim.channel_calib_y0[0],
	             sizeof(m_app_config_interim.channel_calib_y0[0]));
	SETTINGS_SET("channel-2-calib-y0", &m_app_config_interim.channel_calib_y0[1],
	             sizeof(m_app_config_interim.channel_calib_y0[1]));
	SETTINGS_SET("channel-3-calib-y0", &m_app_config_interim.channel_calib_y0[2],
	             sizeof(m_app_config_interim.channel_calib_y0[2]));
	SETTINGS_SET("channel-4-calib-y0", &m_app_config_interim.channel_calib_y0[3],
	             sizeof(m_app_config_interim.channel_calib_y0[3]));

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-calib-x1", &m_app_config_interim.channel_calib_x1[0],
	             sizeof(m_app_config_interim.channel_calib_x1[0]));
	SETTINGS_SET("channel-2-calib-x1", &m_app_config_interim.channel_calib_x1[1],
	             sizeof(m_app_config_interim.channel_calib_x1[1]));
	SETTINGS_SET("channel-3-calib-x1", &m_app_config_interim.channel_calib_x1[2],
	             sizeof(m_app_config_interim.channel_calib_x1[2]));
	SETTINGS_SET("channel-4-calib-x1", &m_app_config_interim.channel_calib_x1[3],
	             sizeof(m_app_config_interim.channel_calib_x1[3]));

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET("channel-1-calib-y1", &m_app_config_interim.channel_calib_y1[0],
	             sizeof(m_app_config_interim.channel_calib_y1[0]));
	SETTINGS_SET("channel-2-calib-y1", &m_app_config_interim.channel_calib_y1[1],
	             sizeof(m_app_config_interim.channel_calib_y1[1]));
	SETTINGS_SET("channel-3-calib-y1", &m_app_config_interim.channel_calib_y1[2],
	             sizeof(m_app_config_interim.channel_calib_y1[2]));
	SETTINGS_SET("channel-4-calib-y1", &m_app_config_interim.channel_calib_y1[3],
	             sizeof(m_app_config_interim.channel_calib_y1[3]));

	SETTINGS_SET("measurement-interval", &m_app_config_interim.measurement_interval,
	             sizeof(m_app_config_interim.measurement_interval));
	SETTINGS_SET("report-interval", &m_app_config_interim.report_interval,
	             sizeof(m_app_config_interim.report_interval));

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

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-active", &m_app_config_interim.channel_active[0],
	            sizeof(m_app_config_interim.channel_active[0]));
	EXPORT_FUNC("channel-2-active", &m_app_config_interim.channel_active[1],
	            sizeof(m_app_config_interim.channel_active[1]));
	EXPORT_FUNC("channel-3-active", &m_app_config_interim.channel_active[2],
	            sizeof(m_app_config_interim.channel_active[2]));
	EXPORT_FUNC("channel-4-active", &m_app_config_interim.channel_active[3],
	            sizeof(m_app_config_interim.channel_active[3]));

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-differential", &m_app_config_interim.channel_differential[0],
	            sizeof(m_app_config_interim.channel_differential[0]));
	EXPORT_FUNC("channel-2-differential", &m_app_config_interim.channel_differential[1],
	            sizeof(m_app_config_interim.channel_differential[1]));
	EXPORT_FUNC("channel-3-differential", &m_app_config_interim.channel_differential[2],
	            sizeof(m_app_config_interim.channel_differential[2]));
	EXPORT_FUNC("channel-4-differential", &m_app_config_interim.channel_differential[3],
	            sizeof(m_app_config_interim.channel_differential[3]));

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-calib-x0", &m_app_config_interim.channel_calib_x0[0],
	            sizeof(m_app_config_interim.channel_calib_x0[0]));
	EXPORT_FUNC("channel-2-calib-x0", &m_app_config_interim.channel_calib_x0[1],
	            sizeof(m_app_config_interim.channel_calib_x0[1]));
	EXPORT_FUNC("channel-3-calib-x0", &m_app_config_interim.channel_calib_x0[2],
	            sizeof(m_app_config_interim.channel_calib_x0[2]));
	EXPORT_FUNC("channel-4-calib-x0", &m_app_config_interim.channel_calib_x0[3],
	            sizeof(m_app_config_interim.channel_calib_x0[3]));

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-calib-y0", &m_app_config_interim.channel_calib_y0[0],
	            sizeof(m_app_config_interim.channel_calib_y0[0]));
	EXPORT_FUNC("channel-2-calib-y0", &m_app_config_interim.channel_calib_y0[1],
	            sizeof(m_app_config_interim.channel_calib_y0[1]));
	EXPORT_FUNC("channel-3-calib-y0", &m_app_config_interim.channel_calib_y0[2],
	            sizeof(m_app_config_interim.channel_calib_y0[2]));
	EXPORT_FUNC("channel-4-calib-y0", &m_app_config_interim.channel_calib_y0[3],
	            sizeof(m_app_config_interim.channel_calib_y0[3]));

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-calib-x1", &m_app_config_interim.channel_calib_x1[0],
	            sizeof(m_app_config_interim.channel_calib_x1[0]));
	EXPORT_FUNC("channel-2-calib-x1", &m_app_config_interim.channel_calib_x1[1],
	            sizeof(m_app_config_interim.channel_calib_x1[1]));
	EXPORT_FUNC("channel-3-calib-x1", &m_app_config_interim.channel_calib_x1[2],
	            sizeof(m_app_config_interim.channel_calib_x1[2]));
	EXPORT_FUNC("channel-4-calib-x1", &m_app_config_interim.channel_calib_x1[3],
	            sizeof(m_app_config_interim.channel_calib_x1[3]));

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC("channel-1-calib-y1", &m_app_config_interim.channel_calib_y1[0],
	            sizeof(m_app_config_interim.channel_calib_y1[0]));
	EXPORT_FUNC("channel-2-calib-y1", &m_app_config_interim.channel_calib_y1[1],
	            sizeof(m_app_config_interim.channel_calib_y1[1]));
	EXPORT_FUNC("channel-3-calib-y1", &m_app_config_interim.channel_calib_y1[2],
	            sizeof(m_app_config_interim.channel_calib_y1[2]));
	EXPORT_FUNC("channel-4-calib-y1", &m_app_config_interim.channel_calib_y1[3],
	            sizeof(m_app_config_interim.channel_calib_y1[3]));

	EXPORT_FUNC("measurement-interval", &m_app_config_interim.measurement_interval,
	            sizeof(m_app_config_interim.measurement_interval));
	EXPORT_FUNC("report-interval", &m_app_config_interim.report_interval,
	            sizeof(m_app_config_interim.report_interval));

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
