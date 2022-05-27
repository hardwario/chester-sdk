#include "app_config.h"

/* CHESTER includes */
#include <ctr_config.h>

/* Zephyr includes */
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <zephyr.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;
static struct app_config m_app_config_interim;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	return 0;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	/*
	int ret;
	const char *next;
	*/

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

#undef EXPORT_FUNC

	return 0;
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

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
