#include "ctr_lrw_v2_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>
#include <chester/ctr_util.h>

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

LOG_MODULE_REGISTER(ctr_lrw_v2_config, CONFIG_CTR_LRW_V2_LOG_LEVEL);

#define SETTINGS_PFX "lrw"

struct ctr_lrw_v2_config g_ctr_lrw_v2_config;

struct ctr_lrw_v2_config m_config = {
	.antenna = CTR_LRW_V2_CONFIG_ANTENNA_INT,
	.band = CTR_LRW_V2_CONFIG_BAND_EU868,
	.class = CTR_LRW_V2_CONFIG_CLASS_A,
	.mode = CTR_LRW_V2_CONFIG_MODE_OTAA,
	.nwk = CTR_LRW_V2_CONFIG_NETWORK_PUBLIC,
	.adr = true,
	.datarate = 0,
	.dutycycle = true,
};

static void print_test(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config test %s", m_config.test ? "true" : "false");
}

static void print_antenna(const struct shell *shell)
{
	switch (m_config.antenna) {
	case CTR_LRW_V2_CONFIG_ANTENNA_INT:
		shell_print(shell, SETTINGS_PFX " config antenna int");
		break;
	case CTR_LRW_V2_CONFIG_ANTENNA_EXT:
		shell_print(shell, SETTINGS_PFX " config antenna ext");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config antenna (unknown)");
	}
}

static void print_band(const struct shell *shell)
{
	switch (m_config.band) {
	case CTR_LRW_V2_CONFIG_BAND_AS923:
		shell_print(shell, SETTINGS_PFX " config band as923");
		break;
	case CTR_LRW_V2_CONFIG_BAND_AU915:
		shell_print(shell, SETTINGS_PFX " config band au915");
		break;
	case CTR_LRW_V2_CONFIG_BAND_EU868:
		shell_print(shell, SETTINGS_PFX " config band eu868");
		break;
	case CTR_LRW_V2_CONFIG_BAND_KR920:
		shell_print(shell, SETTINGS_PFX " config band kr920");
		break;
	case CTR_LRW_V2_CONFIG_BAND_IN865:
		shell_print(shell, SETTINGS_PFX " config band in865");
		break;
	case CTR_LRW_V2_CONFIG_BAND_US915:
		shell_print(shell, SETTINGS_PFX " config band us915");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config band (unknown)");
	}
}

static void print_chmask(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config chmask %s", m_config.chmask);
}

static void print_class(const struct shell *shell)
{
	switch (m_config.class) {
	case CTR_LRW_V2_CONFIG_CLASS_A:
		shell_print(shell, SETTINGS_PFX " config class a");
		break;
	case CTR_LRW_V2_CONFIG_CLASS_C:
		shell_print(shell, SETTINGS_PFX " config class c");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config class (unknown)");
	}
}

static void print_mode(const struct shell *shell)
{
	switch (m_config.mode) {
	case CTR_LRW_V2_CONFIG_MODE_OTAA:
		shell_print(shell, SETTINGS_PFX " config mode otaa");
		break;
	case CTR_LRW_V2_CONFIG_MODE_ABP:
		shell_print(shell, SETTINGS_PFX " config mode abp");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config mode (unknown)");
	}
}

static void print_nwk(const struct shell *shell)
{
	switch (m_config.nwk) {
	case CTR_LRW_V2_CONFIG_NETWORK_PUBLIC:
		shell_print(shell, SETTINGS_PFX " config nwk public");
		break;
	case CTR_LRW_V2_CONFIG_NETWORK_PRIVATE:
		shell_print(shell, SETTINGS_PFX " config nwk private");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config nwk (unknown)");
	}
}

static void print_adr(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config adr %s", m_config.adr ? "true" : "false");
}

static void print_datarate(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config datarate %d", m_config.datarate);
}

static void print_dutycycle(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config dutycycle %s",
		    m_config.dutycycle ? "true" : "false");
}

static void print_devaddr(const struct shell *shell)
{
	char buf[sizeof(m_config.devaddr) * 2 + 1];

	if (ctr_buf2hex(m_config.devaddr, sizeof(m_config.devaddr), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config devaddr %s", buf);
	}
}

static void print_deveui(const struct shell *shell)
{
	char buf[sizeof(m_config.deveui) * 2 + 1];

	if (ctr_buf2hex(m_config.deveui, sizeof(m_config.deveui), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config deveui %s", buf);
	}
}

static void print_joineui(const struct shell *shell)
{
	char buf[sizeof(m_config.joineui) * 2 + 1];

	if (ctr_buf2hex(m_config.joineui, sizeof(m_config.joineui), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config joineui %s", buf);
	}
}

static void print_appkey(const struct shell *shell)
{
	char buf[sizeof(m_config.appkey) * 2 + 1];

	if (ctr_buf2hex(m_config.appkey, sizeof(m_config.appkey), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config appkey %s", buf);
	}
}

static void print_nwkskey(const struct shell *shell)
{
	char buf[sizeof(m_config.nwkskey) * 2 + 1];

	if (ctr_buf2hex(m_config.nwkskey, sizeof(m_config.nwkskey), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config nwkskey %s", buf);
	}
}

static void print_appskey(const struct shell *shell)
{
	char buf[sizeof(m_config.appskey) * 2 + 1];

	if (ctr_buf2hex(m_config.appskey, sizeof(m_config.appskey), buf, sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config appskey %s", buf);
	}
}

int ctr_lrw_v2_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	print_test(shell);
	print_antenna(shell);
	print_band(shell);
	print_chmask(shell);
	print_class(shell);
	print_mode(shell);
	print_nwk(shell);
	print_adr(shell);
	print_datarate(shell);
	print_dutycycle(shell);
	print_devaddr(shell);
	print_deveui(shell);
	print_joineui(shell);
	print_appkey(shell);
	print_nwkskey(shell);
	print_appskey(shell);

	return 0;
}

int ctr_lrw_v2_config_cmd_test(const struct shell *shell, size_t argc, char **argv)
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

int ctr_lrw_v2_config_cmd_antenna(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_antenna(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "int") == 0) {
		m_config.antenna = CTR_LRW_V2_CONFIG_ANTENNA_INT;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "ext") == 0) {
		m_config.antenna = CTR_LRW_V2_CONFIG_ANTENNA_EXT;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_band(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_band(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "as923") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_AS923;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "au915") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_AU915;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "eu868") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_EU868;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "kr920") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_KR920;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "in865") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_IN865;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "us915") == 0) {
		m_config.band = CTR_LRW_V2_CONFIG_BAND_US915;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_chmask(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_chmask(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config.chmask)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			char c = argv[1][i];

			if ((c < '0' || c > '9') && (c < 'a' || c > 'f')) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config.chmask, argv[1]);
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_class(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_class(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "a") == 0) {
		m_config.class = CTR_LRW_V2_CONFIG_CLASS_A;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "c") == 0) {
		m_config.class = CTR_LRW_V2_CONFIG_CLASS_C;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "otaa") == 0) {
		m_config.mode = CTR_LRW_V2_CONFIG_MODE_OTAA;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "abp") == 0) {
		m_config.mode = CTR_LRW_V2_CONFIG_MODE_ABP;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_nwk(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_nwk(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "private") == 0) {
		m_config.nwk = CTR_LRW_V2_CONFIG_NETWORK_PRIVATE;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "public") == 0) {
		m_config.nwk = CTR_LRW_V2_CONFIG_NETWORK_PUBLIC;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_adr(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_adr(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.adr = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.adr = false;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_datarate(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_datarate(shell);
		return 0;
	}

	if (argc == 2) {
		if ((strlen(argv[1]) == 1 && isdigit((int)argv[1][0])) ||
		    (strlen(argv[1]) == 2 && isdigit((int)argv[1][0]) &&
		     isdigit((int)argv[1][1]))) {
			int datarate = atoi(argv[1]);

			if (datarate >= 0 && datarate <= 15) {
				m_config.datarate = datarate;
				return 0;
			}
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_dutycycle(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_dutycycle(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config.dutycycle = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config.dutycycle = false;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_devaddr(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_devaddr(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.devaddr, sizeof(m_config.devaddr), true);
		if (ret == sizeof(m_config.devaddr)) {
			return 0;
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_deveui(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_deveui(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.deveui, sizeof(m_config.deveui), true);
		if (ret == sizeof(m_config.deveui)) {
			return 0;
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_joineui(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_joineui(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.joineui, sizeof(m_config.joineui), true);
		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_appkey(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_appkey(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.appkey, sizeof(m_config.appkey), true);
		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_nwkskey(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_nwkskey(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.nwkskey, sizeof(m_config.nwkskey), true);
		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);

	return -EINVAL;
}

int ctr_lrw_v2_config_cmd_appskey(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_appskey(shell);
		return 0;
	}

	if (argc == 2) {
		ret = ctr_hex2buf(argv[1], m_config.appskey, sizeof(m_config.appskey), true);
		if (ret >= 0) {
			return 0;
		}
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
	SETTINGS_SET_SCALAR("antenna", m_config.antenna);
	SETTINGS_SET_SCALAR("band", m_config.band);
	SETTINGS_SET_ARRAY("chmask", m_config.chmask);
	SETTINGS_SET_SCALAR("class", m_config.class);
	SETTINGS_SET_SCALAR("mode", m_config.mode);
	SETTINGS_SET_SCALAR("nwk", m_config.nwk);
	SETTINGS_SET_SCALAR("adr", m_config.adr);
	SETTINGS_SET_SCALAR("datarate", m_config.datarate);
	SETTINGS_SET_SCALAR("dutycycle", m_config.dutycycle);
	SETTINGS_SET_ARRAY("devaddr", m_config.devaddr);
	SETTINGS_SET_ARRAY("deveui", m_config.deveui);
	SETTINGS_SET_ARRAY("joineui", m_config.joineui);
	SETTINGS_SET_ARRAY("appkey", m_config.appkey);
	SETTINGS_SET_ARRAY("nwkskey", m_config.nwkskey);
	SETTINGS_SET_ARRAY("appskey", m_config.appskey);

#undef SETTINGS_SET_SCALAR
#undef SETTINGS_SET_ARRAY

	return -ENOENT;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	memcpy(&g_ctr_lrw_v2_config, &m_config, sizeof(g_ctr_lrw_v2_config));

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
	EXPORT_FUNC_SCALAR("antenna", m_config.antenna);
	EXPORT_FUNC_SCALAR("band", m_config.band);
	EXPORT_FUNC_ARRAY("chmask", m_config.chmask);
	EXPORT_FUNC_SCALAR("class", m_config.class);
	EXPORT_FUNC_SCALAR("mode", m_config.mode);
	EXPORT_FUNC_SCALAR("nwk", m_config.nwk);
	EXPORT_FUNC_SCALAR("adr", m_config.adr);
	EXPORT_FUNC_SCALAR("datarate", m_config.datarate);
	EXPORT_FUNC_SCALAR("dutycycle", m_config.dutycycle);
	EXPORT_FUNC_ARRAY("devaddr", m_config.devaddr);
	EXPORT_FUNC_ARRAY("deveui", m_config.deveui);
	EXPORT_FUNC_ARRAY("joineui", m_config.joineui);
	EXPORT_FUNC_ARRAY("appkey", m_config.appkey);
	EXPORT_FUNC_ARRAY("nwkskey", m_config.nwkskey);
	EXPORT_FUNC_ARRAY("appskey", m_config.appskey);

#undef EXPORT_FUNC_SCALAR
#undef EXPORT_FUNC_ARRAY

	return 0;
}

static int init()
{
	int ret;

	LOG_INF("System initialiation");

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

	ctr_config_append_show(SETTINGS_PFX, ctr_lrw_v2_config_cmd_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
