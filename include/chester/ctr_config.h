/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_CONFIG_H_
#define CHESTER_INCLUDE_CTR_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>
#include <zephyr/settings/settings.h>

/* Standard include */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_config ctr_config
 * @{
 */

enum ctr_config_item_type {
	CTR_CONFIG_TYPE_INT,
	CTR_CONFIG_TYPE_FLOAT,
	CTR_CONFIG_TYPE_BOOL,
	CTR_CONFIG_TYPE_ENUM,
	CTR_CONFIG_TYPE_STRING,
	CTR_CONFIG_TYPE_HEX,
};

#define CTR_CONFIG_ITEM_INT(_name_d, _var, _min, _max, _help, _default)                            \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_INT,                                                       \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.min = _min,                                                                       \
		.max = _max,                                                                       \
		.help = _help,                                                                     \
		.default_int = _default,                                                           \
	}

#define CTR_CONFIG_ITEM_FLOAT(_name_d, _var, _min, _max, _help, _default)                          \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_FLOAT,                                                     \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.min = _min,                                                                       \
		.max = _max,                                                                       \
		.help = _help,                                                                     \
		.default_float = _default,                                                         \
	}

#define CTR_CONFIG_ITEM_BOOL(_name_d, _var, _help, _default)                                       \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_BOOL,                                                      \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.help = _help,                                                                     \
		.default_bool = _default,                                                          \
	}

#define CTR_CONFIG_ITEM_ENUM(_name_d, _var, _items_str, _help, _default)                           \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_ENUM,                                                      \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.min = 0,                                                                          \
		.max = ARRAY_SIZE(_items_str),                                                     \
		.help = _help,                                                                     \
		.enums = _items_str,                                                               \
		.default_enum = _default,                                                          \
	}

#define CTR_CONFIG_ITEM_STRING(_name_d, _var, _help, _default)                                     \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_STRING,                                                    \
		.variable = _var,                                                                  \
		.size = ARRAY_SIZE(_var),                                                          \
		.help = _help,                                                                     \
		.default_string = _default,                                                        \
	}

#define CTR_CONFIG_ITEM_STRING_PARSE_CB(_name_d, _var, _help, _default, _cb)                       \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_STRING,                                                    \
		.variable = _var,                                                                  \
		.size = ARRAY_SIZE(_var),                                                          \
		.help = _help,                                                                     \
		.default_string = _default,                                                        \
		.parse_cb = _cb,                                                                   \
	}

#define CTR_CONFIG_ITEM_HEX(_name_d, _var, _help, _default)                                        \
	{                                                                                          \
		.module = SETTINGS_PFX,                                                            \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_HEX,                                                       \
		.variable = _var,                                                                  \
		.size = ARRAY_SIZE(_var),                                                          \
		.help = _help,                                                                     \
		.default_hex = _default,                                                           \
	}

struct ctr_config_item;

typedef int (*ctr_config_parse_cb)(const struct shell *shell, char *argv,
				   const struct ctr_config_item *item);

struct ctr_config_item {
	const char *module;
	const char *name;
	enum ctr_config_item_type type;
	void *variable;
	size_t size;
	int min;
	int max;
	const char *help;
	const char **enums;
	union {
		int default_int;
		float default_float;
		bool default_bool;
		int default_enum;
		const char *default_string;
		const uint8_t *default_hex;
	};
	ctr_config_parse_cb parse_cb;
};

typedef int (*ctr_config_show_cb)(const struct shell *shell, size_t argc, char **argv);

int ctr_config_save(bool reboot);
int ctr_config_reset(bool reboot);
void ctr_config_append_show(const char *name, ctr_config_show_cb cb);

int ctr_config_show_item(const struct shell *shell, const struct ctr_config_item *item);
int ctr_config_help_item(const struct shell *shell, const struct ctr_config_item *item);
int ctr_config_parse_item(const struct shell *shell, char *argv,
			  const struct ctr_config_item *item);
int ctr_config_init_item(const struct ctr_config_item *item);

int ctr_config_cmd_config(const struct ctr_config_item *items, int nitems,
			  const struct shell *shell, size_t argc, char **argv);
int ctr_config_h_export(const struct ctr_config_item *items, int nitems,
			int (*export_func)(const char *name, const void *val, size_t val_len));
int ctr_config_h_set(const struct ctr_config_item *items, int nitems, const char *key, size_t len,
		     settings_read_cb read_cb, void *cb_arg);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CONFIG_H_ */
