/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/fs/nvs.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_config, CONFIG_CTR_CONFIG_LOG_LEVEL);

struct show_item {
	const char *name;
	ctr_config_show_cb cb;
	sys_snode_t node;
};

static sys_slist_t m_show_list = SYS_SLIST_STATIC_INIT(&m_show_list);

static int save(void)
{
	int ret;

	ret = settings_save();
	if (ret) {
		LOG_ERR("Call `settings_save` failed: %d", ret);
		return ret;
	}

	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

static int reset(void)
{
	int ret;

	const struct flash_area *fa;
	ret = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
	if (ret) {
		LOG_ERR("Call `flash_area_open` failed: %d", ret);
		return ret;
	}

	ret = flash_area_erase(fa, 0, FIXED_PARTITION_SIZE(storage_partition));
	if (ret < 0) {
		LOG_ERR("Call `flash_area_erase` failed: %d", ret);
		return ret;
	}

	flash_area_close(fa);

	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

int ctr_config_save(void)
{
	int ret;

	ret = save();
	if (ret) {
		LOG_ERR("Call `save` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_config_reset(void)
{
	int ret;

	ret = reset();
	if (ret) {
		LOG_ERR("Call `reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

void ctr_config_append_show(const char *name, ctr_config_show_cb cb)
{
	struct show_item *item = k_malloc(sizeof(*item));
	if (item == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return;
	}

	item->name = name;
	item->cb = cb;

	sys_slist_append(&m_show_list, &item->node);
}

static int cmd_modules(const struct shell *shell, size_t argc, char **argv)
{
	struct show_item *item;
	SYS_SLIST_FOR_EACH_CONTAINER (&m_show_list, item, node) {
		shell_print(shell, "%s", item->name);
	}

	return 0;
}

static int cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	struct show_item *item;
	SYS_SLIST_FOR_EACH_CONTAINER (&m_show_list, item, node) {
		if (item->cb != NULL) {
			ret = item->cb(shell, argc, argv);
			if (ret) {
				LOG_ERR("Call `item->cb` failed: %d", ret);
				shell_error(shell, "command failed");
				return ret;
			}
		}
	}

	return 0;
}

static int cmd_save(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = save();
	if (ret) {
		LOG_ERR("Call `save` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = reset();
	if (ret) {
		LOG_ERR("Call `reset` failed: %d", ret);
		shell_error(shell, "command failed");
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
	sub_config,

	SHELL_CMD_ARG(modules, NULL,
	              "Show all modules.",
	              cmd_modules, 1, 0),

	SHELL_CMD_ARG(show, NULL,
	              "Show all configuration.",
	              cmd_show, 1, 0),

	SHELL_CMD_ARG(save, NULL,
	              "Save all configuration.",
	              cmd_save, 1, 0),

	SHELL_CMD_ARG(reset, NULL,
	              "Reset all configuration.",
	              cmd_reset, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(config, &sub_config, "Configuration commands.", print_help);

/* clang-format on */

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("Call `settings_subsys_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_CONFIG_INIT_PRIORITY);
