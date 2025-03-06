#include <chester/ctr_config.h>

#include "ctr_radon_config.h"

LOG_MODULE_REGISTER(ctr_radon_config, CONFIG_CTR_RADON_LOG_LEVEL);

#define SETTINGS_PFX "radon"

struct ctr_radon_config g_ctr_radon_config;
static struct ctr_radon_config m_config_interim;

/* clang-format off */
static struct ctr_config_item m_config_items[] = {
	CTR_CONFIG_ITEM_INT("modbus-baud", m_config_interim.modbus_baud, 1200, 19200, "Get/Set modbus baudrate.", 19200),
	CTR_CONFIG_ITEM_INT("modbus-addr", m_config_interim.modbus_addr, 1, 247, "Get/Set modbus slave address.", 1),
	CTR_CONFIG_ITEM_ENUM("modbus-parity", m_config_interim.modbus_parity, ((const char *[]){ "none", "odd", "even" }), "Set modbus parity", UART_CFG_PARITY_EVEN),
	CTR_CONFIG_ITEM_ENUM("modbus-stop-bits", m_config_interim.modbus_stop_bits, ((const char *[]){ "", "1", "", "2" }), "Set modbus stop bits", UART_CFG_STOP_BITS_1),
};
/* clang-format on */

int ctr_radon_config_cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

int ctr_radon_config_cmd(const struct shell *shell, size_t argc, char **argv)
{
	return ctr_config_cmd_config(m_config_items, ARRAY_SIZE(m_config_items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	return ctr_config_h_set(m_config_items, ARRAY_SIZE(m_config_items), key, len, read_cb,
				cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_ctr_radon_config, &m_config_interim, sizeof(struct ctr_radon_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	return ctr_config_h_export(m_config_items, ARRAY_SIZE(m_config_items), export_func);
}

int ctr_radon_config_init(void)
{
	int ret;

	LOG_INF("System initialization");

	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_init_item(&m_config_items[i]);
	}

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

	ctr_config_append_show(SETTINGS_PFX, ctr_radon_config_cmd_show);

	return 0;
}
