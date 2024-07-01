/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define SETTINGS_PFX "sample"

struct config {
	int interval;
	enum ctr_led_channel channel;
	char greeting[16 + 1];
};

static struct config m_config;
static struct config m_config_interim;

static const char *m_enum_ctr_led_channel_str[] = {"red", "green", "yellow", "external", "load"};

static const struct ctr_config_item m_config_items[] = {
	CTR_CONFIG_ITEM_INT("interval", m_config_interim.interval, 200, 5000, "blink interval",
			    500),
	CTR_CONFIG_ITEM_ENUM("channel", m_config_interim.channel, m_enum_ctr_led_channel_str,
			     "LED channel", CTR_LED_CHANNEL_R),
	CTR_CONFIG_ITEM_STRING("greeting", m_config_interim.greeting, "greeting text", "Alive"),
};

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

static int cmd_config(const struct shell *shell, size_t argc, char **argv)
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
	memcpy(&m_config, &m_config_interim, sizeof(struct config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	return ctr_config_h_export(m_config_items, ARRAY_SIZE(m_config_items), export_func);
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
	sub_sample,

	SHELL_CMD_ARG(config, NULL,
	              "Configuration commands.",
	              cmd_config, 1, 3),

	SHELL_SUBCMD_SET_END
);

/* clang-format on */

SHELL_CMD_REGISTER(sample, &sub_sample, "Sample commands.", print_help);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

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

	ctr_config_append_show(SETTINGS_PFX, cmd_config_show);

	bool state = false;

	for (;;) {
		LOG_INF("%s", m_config.greeting);

		/* Invert state variable */
		state = !state;

		/* Control LED */
		ctr_led_set(m_config.channel, state);

		/* Wait 500 ms */
		k_sleep(K_MSEC(m_config.interval));
	}

	return 0;
}
