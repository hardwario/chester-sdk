/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_power_config.h"
#include "ctr_power_internal.h"

/* CHESTER includes */
#include <chester/ctr_power.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_batt.h>
#include <chester/drivers/ctr_x10.h>
#include <chester/drivers/ctr_x4.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_power, CONFIG_CTR_POWER_LOG_LEVEL);

#define HAS_CTR_Z    DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_z), okay)
#define HAS_CTR_X4_A DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x4_a), okay)
#define HAS_CTR_X4_B DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x4_b), okay)
#define HAS_CTR_X10  DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x10), okay)

static K_MUTEX_DEFINE(m_lock);

static struct ctr_power_data m_data = {
	.line_voltage = NAN,
	.backup_voltage = NAN,
	.system_voltage_rest = NAN,
	.system_voltage_load = NAN,
	.system_current_load = NAN,
};

static const struct device *m_dev;

static ctr_power_event_cb m_event_cb;
static void *m_event_cb_user_data;

static const char *source_name(enum ctr_power_source source)
{
	switch (source) {
	case CTR_POWER_SOURCE_CTR_Z:
		return "ctr-z";
	case CTR_POWER_SOURCE_CTR_X4_A:
		return "ctr-x4-a";
	case CTR_POWER_SOURCE_CTR_X4_B:
		return "ctr-x4-b";
	case CTR_POWER_SOURCE_CTR_X10:
		return "ctr-x10";
	default:
		return "none";
	}
}

static const char *line_name(enum ctr_power_line line)
{
	switch (line) {
	case CTR_POWER_LINE_DISCONNECTED:
		return "disconnected";
	case CTR_POWER_LINE_CONNECTED:
		return "connected";
	default:
		return "unknown";
	}
}

/* Returns the device backing a source, or NULL when the source is unavailable. */
static const struct device *source_device(enum ctr_power_source source)
{
	switch (source) {
#if HAS_CTR_Z
	case CTR_POWER_SOURCE_CTR_Z:
		return DEVICE_DT_GET(DT_NODELABEL(ctr_z));
#endif
#if HAS_CTR_X4_A
	case CTR_POWER_SOURCE_CTR_X4_A:
		return DEVICE_DT_GET(DT_NODELABEL(ctr_x4_a));
#endif
#if HAS_CTR_X4_B
	case CTR_POWER_SOURCE_CTR_X4_B:
		return DEVICE_DT_GET(DT_NODELABEL(ctr_x4_b));
#endif
#if HAS_CTR_X10
	case CTR_POWER_SOURCE_CTR_X10:
		return DEVICE_DT_GET(DT_NODELABEL(ctr_x10));
#endif
	default:
		return NULL;
	}
}

/* ### Sampling ### */

static int sample_external(void)
{
	int ret = 0;

	enum ctr_power_line line = CTR_POWER_LINE_UNKNOWN;
	float line_voltage = NAN;
	float backup_voltage = NAN;

	switch (m_data.source) {
#if HAS_CTR_Z
	case CTR_POWER_SOURCE_CTR_Z: {
		struct ctr_z_status status;
		ret = ctr_z_get_status(m_dev, &status);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
			goto exit;
		}

		line = status.dc_input_connected ? CTR_POWER_LINE_CONNECTED
						 : CTR_POWER_LINE_DISCONNECTED;

		uint16_t vdc_mv;
		ret = ctr_z_get_vdc_mv(m_dev, &vdc_mv);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
			goto exit;
		}

		line_voltage = vdc_mv / 1000.f;

		uint16_t vbat_mv;
		ret = ctr_z_get_vbat_mv(m_dev, &vbat_mv);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
			goto exit;
		}

		backup_voltage = vbat_mv / 1000.f;
		break;
	}
#endif
#if HAS_CTR_X4_A || HAS_CTR_X4_B
	case CTR_POWER_SOURCE_CTR_X4_A:
	case CTR_POWER_SOURCE_CTR_X4_B: {
		enum ctr_x4_line_state line_state;
		ret = ctr_x4_get_line_state(m_dev, &line_state);
		if (ret) {
			LOG_ERR("Call `ctr_x4_get_line_state` failed: %d", ret);
			goto exit;
		}

		line = (enum ctr_power_line)line_state;

		int line_voltage_mv;
		ret = ctr_x4_get_line_voltage(m_dev, &line_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x4_get_line_voltage` failed: %d", ret);
			goto exit;
		}

		line_voltage = line_voltage_mv / 1000.f;
		/* CHESTER-X4 has no backup battery measurement. */
		break;
	}
#endif
#if HAS_CTR_X10
	case CTR_POWER_SOURCE_CTR_X10: {
		bool line_present;
		ret = ctr_x10_get_line_present(m_dev, &line_present);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_line_present` failed: %d", ret);
			goto exit;
		}

		line = line_present ? CTR_POWER_LINE_CONNECTED : CTR_POWER_LINE_DISCONNECTED;

		int line_voltage_mv;
		ret = ctr_x10_get_line_voltage(m_dev, &line_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_line_voltage` failed: %d", ret);
			goto exit;
		}

		line_voltage = line_voltage_mv / 1000.f;

		int battery_voltage_mv;
		ret = ctr_x10_get_battery_voltage(m_dev, &battery_voltage_mv);
		if (ret) {
			LOG_ERR("Call `ctr_x10_get_battery_voltage` failed: %d", ret);
			goto exit;
		}

		backup_voltage = battery_voltage_mv / 1000.f;
		break;
	}
#endif
	default:
		return 0;
	}

	LOG_INF("Line: %s, voltage: %.2f V", line_name(line), (double)line_voltage);

exit:
	k_mutex_lock(&m_lock, K_FOREVER);
	m_data.line = line;
	m_data.line_voltage = line_voltage;
	m_data.backup_voltage = backup_voltage;
	k_mutex_unlock(&m_lock);

	return ret;
}

static int sample_system(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

	float voltage_rest = NAN;
	float voltage_load = NAN;
	float current_load = NAN;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto exit;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv, CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
		goto exit;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(dev, &load_mv, CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
		goto exit;
	}

	int current_ma;
	ctr_batt_get_load_current_ma(dev, &current_ma, load_mv);

	voltage_rest = rest_mv / 1000.f;
	voltage_load = load_mv / 1000.f;
	current_load = current_ma;

	LOG_INF("Voltage rest: %.2f V, load: %.2f V, current load: %.0f mA", (double)voltage_rest,
		(double)voltage_load, (double)current_load);

exit:
	k_mutex_lock(&m_lock, K_FOREVER);
	m_data.system_voltage_rest = voltage_rest;
	m_data.system_voltage_load = voltage_load;
	m_data.system_current_load = current_load;
	k_mutex_unlock(&m_lock);

	return ret;
}

/*
 * NOTE: Both work items run on the system workqueue, and `sample_system` blocks for
 * roughly 10 seconds inside `ctr_batt`. This is a deliberate temporary trade-off to save
 * RAM; move to a dedicated queue once the battery measurement is made asynchronous.
 */
static void sample_work_handler(struct k_work *work)
{
	int ret;

	ret = sample_external();
	if (ret) {
		LOG_ERR("Call `sample_external` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_sample_work, sample_work_handler);

static void sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit(&m_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_sample_timer, sample_timer_handler, NULL);

static void system_work_handler(struct k_work *work)
{
	int ret;

	ret = sample_system();
	if (ret) {
		LOG_ERR("Call `sample_system` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_system_work, system_work_handler);

static void system_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit(&m_system_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_system_timer, system_timer_handler, NULL);

/* ### Event handling ### */

static bool should_report(enum ctr_power_line line)
{
	switch (line) {
	case CTR_POWER_LINE_CONNECTED:
		return g_ctr_power_config.report_connected;
	case CTR_POWER_LINE_DISCONNECTED:
		return g_ctr_power_config.report_disconnected;
	default:
		return false;
	}
}

static void handle_line_change(enum ctr_power_line line)
{
	struct ctr_power_event event = {.line = line};

	if (ctr_rtc_get_ts(&event.timestamp)) {
		event.timestamp = 0;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	m_data.line = line;

	if (m_data.event_count == ARRAY_SIZE(m_data.events)) {
		/* Ring buffer: drop the oldest so the newest is always kept. */
		memmove(m_data.events, &m_data.events[1],
			(ARRAY_SIZE(m_data.events) - 1) * sizeof(m_data.events[0]));
		m_data.event_count--;
		LOG_WRN("Event buffer full, dropping the oldest event");
	}

	m_data.events[m_data.event_count++] = event;

	LOG_INF("Event: %s, count: %d", line_name(line), m_data.event_count);

	k_mutex_unlock(&m_lock);

	/*
	 * Refresh the voltages so a report triggered below carries fresh values. This only
	 * works because the application's rate limiter delays the actual send; the resample
	 * is not awaited here.
	 */
	k_work_submit(&m_sample_work);

	if (m_event_cb && should_report(line)) {
		m_event_cb(&event, m_event_cb_user_data);
	}
}

#if HAS_CTR_Z
static void handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	switch (event) {
	case CTR_Z_EVENT_DC_CONNECTED:
		handle_line_change(CTR_POWER_LINE_CONNECTED);
		break;
	case CTR_Z_EVENT_DC_DISCONNECTED:
		handle_line_change(CTR_POWER_LINE_DISCONNECTED);
		break;
	default:
		break;
	}
}
#endif

#if HAS_CTR_X4_A || HAS_CTR_X4_B
static void handler_ctr_x4(const struct device *dev, enum ctr_x4_event event, void *user_data)
{
	switch (event) {
	case CTR_X4_EVENT_LINE_CONNECTED:
		handle_line_change(CTR_POWER_LINE_CONNECTED);
		break;
	case CTR_X4_EVENT_LINE_DISCONNECTED:
		handle_line_change(CTR_POWER_LINE_DISCONNECTED);
		break;
	default:
		break;
	}
}
#endif

#if HAS_CTR_X10
static void handler_ctr_x10(const struct device *dev, enum ctr_x10_event event, void *user_data)
{
	switch (event) {
	case CTR_X10_EVENT_LINE_CONNECTED:
		handle_line_change(CTR_POWER_LINE_CONNECTED);
		break;
	case CTR_X10_EVENT_LINE_DISCONNECTED:
		handle_line_change(CTR_POWER_LINE_DISCONNECTED);
		break;
	default:
		break;
	}
}
#endif

/* Registers the event handler and does any source-specific bring-up. */
static int attach_source(void)
{
	int ret;

	switch (m_data.source) {
#if HAS_CTR_Z
	case CTR_POWER_SOURCE_CTR_Z:
		ret = ctr_z_set_handler(m_dev, handler_ctr_z, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
			return ret;
		}

		/* Do not assume the application enabled these. */
		ret = ctr_z_enable_interrupts(m_dev);
		if (ret) {
			LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
			return ret;
		}
		break;
#endif
#if HAS_CTR_X4_A || HAS_CTR_X4_B
	case CTR_POWER_SOURCE_CTR_X4_A:
	case CTR_POWER_SOURCE_CTR_X4_B:
		ret = ctr_x4_set_handler(m_dev, handler_ctr_x4, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_handler` failed: %d", ret);
			return ret;
		}
		break;
#endif
#if HAS_CTR_X10
	case CTR_POWER_SOURCE_CTR_X10:
		ret = ctr_x10_set_handler(m_dev, handler_ctr_x10, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_x10_set_handler` failed: %d", ret);
			return ret;
		}
		break;
#endif
	default:
		break;
	}

	return 0;
}

/* ### Detection ### */

static enum ctr_power_source source_from_config(enum ctr_power_config_source source)
{
	switch (source) {
	case CTR_POWER_CONFIG_SOURCE_CTR_Z:
		return CTR_POWER_SOURCE_CTR_Z;
	case CTR_POWER_CONFIG_SOURCE_CTR_X4_A:
		return CTR_POWER_SOURCE_CTR_X4_A;
	case CTR_POWER_CONFIG_SOURCE_CTR_X4_B:
		return CTR_POWER_SOURCE_CTR_X4_B;
	case CTR_POWER_CONFIG_SOURCE_CTR_X10:
		return CTR_POWER_SOURCE_CTR_X10;
	default:
		return CTR_POWER_SOURCE_NONE;
	}
}

static void set_source(enum ctr_power_source source)
{
	m_dev = source_device(source);
	m_data.source = m_dev ? source : CTR_POWER_SOURCE_NONE;
}

/* Honours an explicit configuration, warning and falling back when the device is missing. */
static void set_source_checked(enum ctr_power_source source)
{
	const struct device *dev = source_device(source);

	if (!dev || !device_is_ready(dev)) {
		LOG_WRN("Source `%s` not available, using `none`", source_name(source));
		set_source(CTR_POWER_SOURCE_NONE);
		return;
	}

	set_source(source);
}

static void detect_source(void)
{
	if (g_ctr_power_config.source != CTR_POWER_CONFIG_SOURCE_AUTO) {
		set_source_checked(source_from_config(g_ctr_power_config.source));
		return;
	}

	/* CHESTER-Z wins when its driver came up; it is probed over I2C. */
#if HAS_CTR_Z
	if (device_is_ready(DEVICE_DT_GET(DT_NODELABEL(ctr_z)))) {
		set_source(CTR_POWER_SOURCE_CTR_Z);
		return;
	}
#endif

	/* Otherwise exactly one of x4a/x4b/x10 must be enabled in devicetree. */
	if (HAS_CTR_X4_A + HAS_CTR_X4_B + HAS_CTR_X10 != 1) {
		LOG_WRN("Cannot pick a power source automatically, using `none`");
		set_source(CTR_POWER_SOURCE_NONE);
		return;
	}

#if HAS_CTR_X4_A
	set_source_checked(CTR_POWER_SOURCE_CTR_X4_A);
#elif HAS_CTR_X4_B
	set_source_checked(CTR_POWER_SOURCE_CTR_X4_B);
#elif HAS_CTR_X10
	set_source_checked(CTR_POWER_SOURCE_CTR_X10);
#else
	set_source(CTR_POWER_SOURCE_NONE);
#endif
}

/* ### Public API ### */

int ctr_power_init(ctr_power_event_cb cb, void *user_data)
{
	int ret;

	LOG_INF("System initialization");

	m_event_cb = cb;
	m_event_cb_user_data = user_data;

	detect_source();

	LOG_INF("Source: %s", source_name(m_data.source));

	ret = attach_source();
	if (ret) {
		LOG_ERR("Call `attach_source` failed: %d", ret);
		return ret;
	}

	k_timer_start(&m_sample_timer, K_NO_WAIT, K_SECONDS(g_ctr_power_config.interval_sample));

	/*
	 * The system measurement is expensive and changes slowly, so it runs on a fixed
	 * long interval; this matches what the applications did before.
	 */
	k_timer_start(&m_system_timer, K_SECONDS(60), K_HOURS(12));

	return 0;
}

enum ctr_power_source ctr_power_get_source(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
	enum ctr_power_source source = m_data.source;
	k_mutex_unlock(&m_lock);

	return source;
}

struct ctr_power_data *ctr_power_data_lock(void)
{
	k_mutex_lock(&m_lock, K_FOREVER);
	return &m_data;
}

void ctr_power_data_unlock(void)
{
	k_mutex_unlock(&m_lock);
}

void ctr_power_data_clear_events(struct ctr_power_data *data)
{
	data->event_count = 0;
}

#if defined(CONFIG_CTR_POWER_SHELL)

static void print_voltage(const struct shell *shell, const char *name, float value)
{
	if (isnan(value)) {
		shell_print(shell, "%s: unknown", name);
		return;
	}

	shell_print(shell, "%s: %.2f V", name, (double)value);
}

static int cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	k_mutex_lock(&m_lock, K_FOREVER);

	shell_print(shell, "source: %s", source_name(m_data.source));
	shell_print(shell, "line: %s", line_name(m_data.line));
	print_voltage(shell, "line voltage", m_data.line_voltage);
	print_voltage(shell, "backup voltage", m_data.backup_voltage);
	print_voltage(shell, "system voltage rest", m_data.system_voltage_rest);
	print_voltage(shell, "system voltage load", m_data.system_voltage_load);

	if (isnan(m_data.system_current_load)) {
		shell_print(shell, "system current load: unknown");
	} else {
		shell_print(shell, "system current load: %.0f mA",
			    (double)m_data.system_current_load);
	}

	shell_print(shell, "events: %d", m_data.event_count);

	for (int i = 0; i < m_data.event_count; i++) {
		shell_print(shell, "event %d: %lld %s", i, m_data.events[i].timestamp,
			    line_name(m_data.events[i].line));
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = sample_external();
	if (ret) {
		shell_error(shell, "Call `sample_external` failed: %d", ret);
		return ret;
	}

	ret = sample_system();
	if (ret) {
		shell_error(shell, "Call `sample_system` failed: %d", ret);
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
	sub_power,

	SHELL_CMD_ARG(config, NULL,
	              "Configuration commands.",
	              ctr_power_config_cmd, 1, 3),

	SHELL_CMD_ARG(show, NULL, "Show power metrics.", cmd_show, 1, 0),
	SHELL_CMD_ARG(sample, NULL, "Sample all power metrics.", cmd_sample, 1, 0),

	SHELL_SUBCMD_SET_END
);
/* clang-format on */

SHELL_CMD_REGISTER(power, &sub_power, "Power commands.", print_help);

#endif /* defined(CONFIG_CTR_POWER_SHELL) */
