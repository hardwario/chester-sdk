#include <ctr_config.h>

/* Zephyr includes */
#include <init.h>
#include <fs/nvs.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <storage/flash_map.h>
#include <sys/reboot.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(hio_config, LOG_LEVEL_DBG);

struct show_item {
	const char *name;
	hio_config_show_cb cb;
	sys_snode_t node;
};

static sys_slist_t m_show_list = SYS_SLIST_STATIC_INIT(&m_show_list);

void hio_config_append_show(const char *name, hio_config_show_cb cb)
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
			if (ret < 0) {
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

	ret = settings_save();

	if (ret < 0) {
		LOG_ERR("Call `settings_save` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	const struct flash_area *fa;

	ret = flash_area_open(FLASH_AREA_ID(storage), &fa);

	if (ret < 0) {
		LOG_ERR("Call `flash_area_open` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	uint32_t sector_count = 1;
	struct flash_sector flash_sector;

	ret = flash_area_get_sectors(FLASH_AREA_ID(storage), &sector_count, &flash_sector);

	if (ret < 0 && ret != -ENOMEM) {
		LOG_ERR("Call `flash_area_get_sectors` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	uint16_t nvs_sector_size = CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT * flash_sector.fs_size;

	if (nvs_sector_size > UINT16_MAX) {
		return -EDOM;
	}

	size_t nvs_size = 0;
	uint16_t nvs_sector_count = 0;

	while (nvs_sector_count < CONFIG_SETTINGS_NVS_SECTOR_COUNT) {
		nvs_size += nvs_sector_size;

		if (nvs_size > fa->fa_size) {
			break;
		}

		nvs_sector_count++;
	}

	struct nvs_fs fs = { .sector_size = nvs_sector_size,
		             .sector_count = nvs_sector_count,
		             .offset = fa->fa_off };

	ret = nvs_init(&fs, fa->fa_dev_name);

	if (ret < 0) {
		LOG_ERR("Call `nvs_init` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	ret = nvs_clear(&fs);

	if (ret < 0) {
		LOG_ERR("Call `nvs_clear` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	sys_reboot(SYS_REBOOT_COLD);

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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_config,
                               SHELL_CMD_ARG(modules, NULL, "Show all modules.", cmd_modules, 1, 0),
                               SHELL_CMD_ARG(show, NULL, "Show all configuration.", cmd_show, 1, 0),
                               SHELL_CMD_ARG(save, NULL, "Save all configuration.", cmd_save, 1, 0),
                               SHELL_CMD_ARG(reset, NULL, "Reset all configuration.", cmd_reset, 1,
                                             0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(config, &sub_config, "Configuration commands.", print_help);

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	ret = settings_subsys_init();

	if (ret < 0) {
		LOG_ERR("Call `settings_subsys_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_HIO_CONFIG_INIT_PRIORITY);
