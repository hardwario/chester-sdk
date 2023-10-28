/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_edge.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_button, CONFIG_CTR_BUTTON_LOG_LEVEL);

static const struct gpio_dt_spec m_button_int = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec m_button_ext = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static int read_button(enum ctr_button_channel channel, bool *is_pressed)
{
	int ret;

	switch (channel) {
	case CTR_BUTTON_CHANNEL_INT:
		ret = gpio_pin_get_dt(&m_button_int);
		break;

	case CTR_BUTTON_CHANNEL_EXT:
		ret = gpio_pin_get_dt(&m_button_ext);
		break;

	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_get_dt` failed: %d", ret);
		return ret;
	}

	*is_pressed = ret == 0 ? false : true;

	return 0;
}

static int cmd_button_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum ctr_button_channel channel;

	if (strcmp(argv[1], "int") == 0) {
		channel = CTR_BUTTON_CHANNEL_INT;

	} else if (strcmp(argv[1], "ext") == 0) {
		channel = CTR_BUTTON_CHANNEL_EXT;

	} else {
		shell_error(shell, "invalid channel name");
		shell_help(shell);
		return -EINVAL;
	}

	bool is_pressed;
	ret = read_button(channel, &is_pressed);
	if (ret) {
		LOG_ERR("Call `read_button` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "pressed: %s", is_pressed ? "true" : "false");

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
	sub_button,

	SHELL_CMD_ARG(read, NULL,
	              "Read button state (format int|ext).",
	              cmd_button_read, 2, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(button, &sub_button, "Button commands.", print_help);

/* clang-format on */

#define MAX_CLICK_PERIOD 600
#define MIN_PRESS_LENGTH 1000

static struct ctr_edge m_edge_int;
static struct ctr_edge m_edge_ext;

struct button_data {
	int64_t last_event;
	int click_count;
	int64_t press_length;

	struct k_work click_breakup_work;
	struct k_work click_work;
	struct k_work hold_work;
	struct k_timer breakup_timer;
};

static struct button_data m_button_data_int = {0};
static struct button_data m_button_data_ext = {0};

static ctr_button_event_cb m_event_cb = NULL;
static void *m_user_data = NULL;

static enum ctr_button_channel button_data_to_channel(struct button_data *data)
{
	return data == &m_button_data_int ? CTR_BUTTON_CHANNEL_INT : CTR_BUTTON_CHANNEL_EXT;
}

static void click_breakup_work_handler(struct k_work *work)
{
	struct button_data *data = CONTAINER_OF(work, struct button_data, click_breakup_work);

	m_event_cb(button_data_to_channel(data), CTR_BUTTON_EVENT_CLICK, data->click_count,
		m_user_data);
	data->click_count = 0;
}

static void click_breakup_timer_handler(struct k_timer *timer)
{
	struct button_data *data = CONTAINER_OF(timer, struct button_data, breakup_timer);

	k_work_submit(&data->click_breakup_work);
}

static void click_work_handler(struct k_work *work)
{
	struct button_data *data = CONTAINER_OF(work, struct button_data, click_work);

	k_timer_start(&data->breakup_timer, K_MSEC(MAX_CLICK_PERIOD), K_FOREVER);
}

static void hold_work_handler(struct k_work *work)
{
	struct button_data *data = CONTAINER_OF(work, struct button_data, hold_work);

	m_event_cb(button_data_to_channel(data), CTR_BUTTON_EVENT_HOLD, data->press_length,
		m_user_data);
}

static void edge_event_cb(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	if (!m_event_cb)
		return;

	struct button_data *data = edge == &m_edge_int ? &m_button_data_int : &m_button_data_ext;

	int64_t uptime = k_uptime_get();
	int64_t diff = uptime - data->last_event;

	if (event == CTR_EDGE_EVENT_INACTIVE) {
		if (diff > MIN_PRESS_LENGTH) {
			data->press_length = diff;
			k_work_submit(&data->hold_work);
		} else {
			k_work_submit(&data->click_work);
			data->click_count++;
		}
	}

	data->last_event = uptime;
}

int ctr_button_set_event_cb(ctr_button_event_cb cb, void *user_data)
{
	int ret;

	m_event_cb = cb;
	m_user_data = user_data;

	ret = ctr_edge_watch(&m_edge_int);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_watch(&m_edge_ext);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	if (!device_is_ready(m_button_int.port)) {
		LOG_ERR("Device `BUTTON_INT` not ready");
		return -EINVAL;
	}

	if (!device_is_ready(m_button_ext.port)) {
		LOG_ERR("Device `BUTTON_EXT` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&m_button_int, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&m_button_ext, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	k_work_init(&m_button_data_int.click_breakup_work, click_breakup_work_handler);
	k_work_init(&m_button_data_int.click_work, click_work_handler);
	k_work_init(&m_button_data_int.hold_work, hold_work_handler);
	k_timer_init(&m_button_data_int.breakup_timer, click_breakup_timer_handler, NULL);
	k_work_init(&m_button_data_ext.click_breakup_work, click_breakup_work_handler);
	k_work_init(&m_button_data_ext.click_work, click_work_handler);
	k_work_init(&m_button_data_ext.hold_work, hold_work_handler);
	k_timer_init(&m_button_data_ext.breakup_timer, click_breakup_timer_handler, NULL);

	ret = ctr_edge_init(&m_edge_int, &m_button_int, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(&m_edge_int, edge_event_cb, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(&m_edge_int, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(&m_edge_int, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(&m_edge_int, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` (BUTTON_INT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_init(&m_edge_ext, &m_button_ext, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(&m_edge_ext, edge_event_cb, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(&m_edge_ext, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(&m_edge_ext, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(&m_edge_ext, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` (BUTTON_EXT) failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_BUTTON_INIT_PRIORITY);
