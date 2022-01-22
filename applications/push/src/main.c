#include "app_config.h"

/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_buf.h>
#include <ctr_gpio.h>
#include <ctr_hygro.h>
#include <ctr_led.h>
#include <ctr_lrw.h>
#include <ctr_rtc.h>
#include <ctr_therm.h>
#include <drivers/ctr_batt.h>
#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/watchdog.h>
#include <logging/log.h>
#include <random/rand32.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* TODO Would be nice to define using K_SECONDS, etc. Proper macros? */
#define BATT_TEST_INTERVAL_MSEC (6 * 60 * 60 * 1000)
#define REPORT_INTERVAL_MSEC (15 * 60 * 1000)

struct data_errors {
	bool orientation;
	bool int_temperature;
	bool ext_temperature;
	bool ext_humidity;
	bool line_present;
	bool line_voltage;
	bool bckp_voltage;
	bool batt_voltage_rest;
	bool batt_voltage_load;
	bool batt_current_load;
};

struct data_states {
	int orientation;
	float int_temperature;
	float ext_temperature;
	float ext_humidity;
	bool line_present;
	float line_voltage;
	float bckp_voltage;
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
};

struct data_events {
	atomic_t device_tilt;
	atomic_t button_x_click;
	atomic_t button_x_hold;
	atomic_t button_1_click;
	atomic_t button_1_hold;
	atomic_t button_2_click;
	atomic_t button_2_hold;
	atomic_t button_3_click;
	atomic_t button_3_hold;
	atomic_t button_4_click;
	atomic_t button_4_hold;
};

struct data_sources {
	atomic_t device_boot;
	atomic_t button_x_click;
	atomic_t button_x_hold;
	atomic_t button_1_click;
	atomic_t button_1_hold;
	atomic_t button_2_click;
	atomic_t button_2_hold;
	atomic_t button_3_click;
	atomic_t button_3_hold;
	atomic_t button_4_click;
	atomic_t button_4_hold;
};

struct data {
	struct data_errors errors;
	struct data_states states;
	struct data_events events;
	struct data_sources sources;
};

static struct data m_data = {
	.errors = {
		.orientation = true,
		.int_temperature = true,
		.ext_temperature = true,
		.ext_humidity = true,
		.line_present = true,
		.line_voltage = true,
		.bckp_voltage = true,
		.batt_voltage_rest = true,
		.batt_voltage_load = true,
		.batt_current_load = true,
        },
};

static const struct device *m_batt_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));
static const struct device *m_ctr_z_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));
static const struct device *m_wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));

static struct data m_data;
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static atomic_t m_send = true;

static int task_battery(void)
{
	int ret = 0;

	if (!device_is_ready(m_batt_dev)) {
		LOG_ERR("Device `CTR_BATT` not ready");
		return -EINVAL;
	}

	static int64_t next;

	if (k_uptime_get() >= next) {
		int voltage_rest_mv;
		ret = ctr_batt_get_rest_voltage_mv(m_batt_dev, &voltage_rest_mv,
		                                   CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
			goto error;
		}

		int voltage_load_mv;
		ret = ctr_batt_get_load_voltage_mv(m_batt_dev, &voltage_load_mv,
		                                   CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
			goto error;
		}

		int current_load_ma;
		ctr_batt_get_load_current_ma(m_batt_dev, &current_load_ma, voltage_load_mv);

		LOG_INF("Battery rest voltage: %u mV", voltage_rest_mv);
		LOG_INF("Battery load voltage: %u mV", voltage_load_mv);
		LOG_INF("Battery load current: %u mA", current_load_ma);

		next = k_uptime_get() + BATT_TEST_INTERVAL_MSEC;

		m_data.errors.batt_voltage_rest = false;
		m_data.errors.batt_voltage_load = false;
		m_data.errors.batt_current_load = false;

		m_data.states.batt_voltage_rest = voltage_rest_mv / 1000.f;
		m_data.states.batt_voltage_load = voltage_load_mv / 1000.f;
		m_data.states.batt_current_load = current_load_ma;
	}

	return 0;

error:
	m_data.errors.batt_voltage_rest = true;
	m_data.errors.batt_voltage_load = true;
	m_data.errors.batt_current_load = true;

	return ret;
}

static int handle_button(enum ctr_z_event event, enum ctr_z_event match,
                         enum ctr_z_led_channel led_channel, enum ctr_z_led_command led_command,
                         enum ctr_z_buzzer_command buzzer_command, atomic_t *counter,
                         atomic_t *source)
{
	int ret;

	if (event != match) {
		return 0;
	}

	if (!device_is_ready(m_ctr_z_dev)) {
		LOG_ERR("Device `CTR_Z` not ready");
		return -EINVAL;
	}

	struct ctr_z_led_param led_param = {
		.brightness = CTR_Z_LED_BRIGHTNESS_HIGH,
		.command = led_command,
		.pattern = CTR_Z_LED_PATTERN_NONE,
	};

	ret = ctr_z_set_led(m_ctr_z_dev, led_channel, &led_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		return ret;
	}

	struct ctr_z_buzzer_param buzzer_param = {
		.command = buzzer_command,
		.pattern = CTR_Z_BUZZER_PATTERN_NONE,
	};

	ret = ctr_z_set_buzzer(m_ctr_z_dev, &buzzer_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_buzzer` failed: %d", ret);
		return ret;
	}

	atomic_inc(counter);
	atomic_set(source, 1);
	atomic_set(&m_send, true);

	k_sem_give(&m_loop_sem);

	return 1;
}

static void ctr_z_handler(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

#define HANDLE_CLICK(button, counter, source)                                                      \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button##_CLICK,                    \
		                    CTR_Z_LED_CHANNEL_##button##_R, CTR_Z_LED_COMMAND_1X_1_2,      \
		                    CTR_Z_BUZZER_COMMAND_1X_1_2, counter, source);                 \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_CLICK(0, &m_data.events.button_x_click, &m_data.sources.button_x_click);
	HANDLE_CLICK(1, &m_data.events.button_1_click, &m_data.sources.button_1_click);
	HANDLE_CLICK(2, &m_data.events.button_2_click, &m_data.sources.button_2_click);
	HANDLE_CLICK(3, &m_data.events.button_3_click, &m_data.sources.button_3_click);
	HANDLE_CLICK(4, &m_data.events.button_4_click, &m_data.sources.button_4_click);

#undef HANDLE_CLICK

#define HANDLE_HOLD(button, counter, source)                                                       \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button##_HOLD,                     \
		                    CTR_Z_LED_CHANNEL_##button##_G, CTR_Z_LED_COMMAND_2X_1_2,      \
		                    CTR_Z_BUZZER_COMMAND_2X_1_2, counter, source);                 \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_HOLD(0, &m_data.events.button_x_hold, &m_data.sources.button_x_hold);
	HANDLE_HOLD(1, &m_data.events.button_1_hold, &m_data.sources.button_1_hold);
	HANDLE_HOLD(2, &m_data.events.button_2_hold, &m_data.sources.button_2_hold);
	HANDLE_HOLD(3, &m_data.events.button_3_hold, &m_data.sources.button_3_hold);
	HANDLE_HOLD(4, &m_data.events.button_4_hold, &m_data.sources.button_4_hold);

#undef HANDLE_HOLD

	switch (event) {
	case CTR_Z_EVENT_DC_CONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_CONNECTED`");
		atomic_set(&m_send, true);
		k_sem_give(&m_loop_sem);
		break;

	case CTR_Z_EVENT_DC_DISCONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_DISCONNECTED`");
		atomic_set(&m_send, true);
		break;
	default:
		break;
	}

apply:
	ret = ctr_z_apply(m_ctr_z_dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
	}
}

static int init_chester_z(void)
{
	int ret;

	if (!device_is_ready(m_ctr_z_dev)) {
		LOG_ERR("Device `CTR_Z` not ready");
		return -ENODEV;
	}

	ret = ctr_z_set_handler(m_ctr_z_dev, ctr_z_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_z_get_serial_number(m_ctr_z_dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		return ret;
	}

	LOG_INF("Serial number: %08x", serial_number);

	uint16_t hw_revision;
	ret = ctr_z_get_hw_revision(m_ctr_z_dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW revision: %04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_z_get_hw_variant(m_ctr_z_dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW variant: %08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_z_get_fw_version(m_ctr_z_dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		return ret;
	}

	LOG_INF("FW version: %08x", fw_version);

	char vendor_name[64];
	ret = ctr_z_get_vendor_name(m_ctr_z_dev, vendor_name, sizeof(vendor_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vendor_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Vendor name: %s", vendor_name);

	char product_name[64];
	ret = ctr_z_get_product_name(m_ctr_z_dev, product_name, sizeof(product_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_product_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Product name: %s", product_name);

	return 0;
}

static int task_chester_z(void)
{
	int ret;

	struct ctr_z_status status;
	ret = ctr_z_get_status(m_ctr_z_dev, &status);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
		goto error;
	}

	LOG_INF("DC input connected: %d", (int)status.dc_input_connected);

	uint16_t vdc;
	ret = ctr_z_get_vdc_mv(m_ctr_z_dev, &vdc);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage DC input: %u mV", vdc);

	uint16_t vbat;
	ret = ctr_z_get_vbat_mv(m_ctr_z_dev, &vbat);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage backup battery: %u mV", vbat);

	m_data.errors.line_present = false;
	m_data.errors.line_voltage = false;
	m_data.errors.bckp_voltage = false;

	m_data.states.line_present = status.dc_input_connected;
	m_data.states.line_voltage = vdc / 1000.f;
	m_data.states.bckp_voltage = vbat / 1000.f;

	return 0;

error:
	m_data.errors.line_present = true;
	m_data.errors.line_voltage = true;
	m_data.errors.bckp_voltage = true;

	return ret;
}

static int init_sensors(void)
{
	int ret;

	ret = ctr_therm_init();
	if (ret) {
		LOG_ERR("Call `ctr_therm_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int task_sensors(void)
{
	int ret;

	bool error = false;

	ret = ctr_accel_read(NULL, NULL, NULL, &m_data.states.orientation);

	if (ret < 0) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		m_data.errors.orientation = true;
		error = true;
	} else {
		LOG_INF("Orientation: %d", m_data.states.orientation);
		m_data.errors.orientation = false;
	}

	ret = ctr_therm_read(&m_data.states.int_temperature);

	if (ret < 0) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		m_data.errors.int_temperature = true;
		error = true;
	} else {
		LOG_INF("Int. temperature: %.1f C", m_data.states.int_temperature);
		m_data.errors.int_temperature = false;
	}

#if IS_ENABLED(CONFIG_CTR_HYGRO)
	ret = ctr_hygro_read(&m_data.states.ext_temperature, &m_data.states.ext_humidity);

	if (ret < 0) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		m_data.errors.ext_temperature = true;
		m_data.errors.ext_humidity = true;
		error = true;
	} else {
		LOG_INF("Ext. temperature: %.1f C", m_data.states.ext_temperature);
		LOG_INF("Ext. humidity: %.1f %%", m_data.states.ext_humidity);
		m_data.errors.ext_temperature = false;
		m_data.errors.ext_humidity = false;
	}
#else
	m_data.errors.ext_temperature = true;
	m_data.errors.ext_humidity = true;
#endif

	return error ? -EIO : 0;
}

static void lrw_event_handler(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param)
{
	ARG_UNUSED(data);
	ARG_UNUSED(param);

	int ret;

	switch (event) {
	case CTR_LRW_EVENT_FAILURE:
		LOG_INF("Event `CTR_LRW_EVENT_FAILURE`");
		ret = ctr_lrw_start(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_OK:
		LOG_INF("Event `CTR_LRW_EVENT_START_OK`");
		ret = ctr_lrw_join(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_join` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_START_ERR`");
		break;
	case CTR_LRW_EVENT_JOIN_OK:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_OK`");
		break;
	case CTR_LRW_EVENT_JOIN_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_ERR`");
		break;
	case CTR_LRW_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_OK`");
		break;
	case CTR_LRW_EVENT_SEND_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_ERR`");
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static int compose(struct ctr_buf *buf, const struct data *data)
{
	int ret = 0;

	ctr_buf_reset(buf);

	uint32_t flags = 0;

	flags |= atomic_clear(&m_data.sources.button_x_click) ? BIT(0) : 0;
	flags |= atomic_clear(&m_data.sources.button_x_hold) ? BIT(1) : 0;
	flags |= atomic_clear(&m_data.sources.button_1_click) ? BIT(2) : 0;
	flags |= atomic_clear(&m_data.sources.button_1_hold) ? BIT(3) : 0;
	flags |= atomic_clear(&m_data.sources.button_2_click) ? BIT(4) : 0;
	flags |= atomic_clear(&m_data.sources.button_2_hold) ? BIT(5) : 0;
	flags |= atomic_clear(&m_data.sources.button_3_click) ? BIT(6) : 0;
	flags |= atomic_clear(&m_data.sources.button_3_hold) ? BIT(7) : 0;
	flags |= atomic_clear(&m_data.sources.button_4_click) ? BIT(8) : 0;
	flags |= atomic_clear(&m_data.sources.button_4_hold) ? BIT(9) : 0;

	flags |= data->states.line_present ? BIT(29) : 0;
	flags |= data->errors.line_present ? BIT(30) : 0;

	flags |= atomic_clear(&m_data.sources.device_boot) ? BIT(31) : 0;

	ret |= ctr_buf_append_u32(buf, flags);

	if (data->errors.orientation) {
		ret |= ctr_buf_append_u8(buf, 0);
	} else {
		ret |= ctr_buf_append_u8(buf, data->states.orientation);
	}

	if (data->errors.int_temperature) {
		ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
	} else {
		int16_t val = data->states.int_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_temperature) {
		ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
	} else {
		int16_t val = data->states.ext_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_humidity) {
		ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
	} else {
		uint8_t val = data->states.ext_humidity * 2.f;
		ret |= ctr_buf_append_u8(buf, val);
	}

	if (data->errors.line_voltage) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.line_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.bckp_voltage) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.bckp_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_rest) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_voltage_rest * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_load) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_voltage_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_current_load) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_current_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.device_tilt));

	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_x_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_x_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_1_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_1_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_2_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_2_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_3_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_3_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_4_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_4_hold));

	if (ret != 0) {
		return -ENOSPC;
	}

	return 0;
}

void send_timer(struct k_timer *timer_id)
{
	LOG_INF("Send timer expired");

	atomic_set(&m_send, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_send_timer, send_timer, NULL);

static int send(void)
{
	int ret;

	int64_t jitter = (int32_t)sys_rand32_get() % (REPORT_INTERVAL_MSEC / 5);
	int64_t duration = REPORT_INTERVAL_MSEC + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&m_send_timer, Z_TIMEOUT_MS(duration), K_FOREVER);

	CTR_BUF_DEFINE_STATIC(buf, 51);

	ret = compose(&buf, &m_data);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	struct ctr_lrw_send_opts opts = CTR_LRW_SEND_OPTS_DEFAULTS;

	ret = ctr_lrw_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int loop(void)
{
	int ret;

	ret = task_battery();
	if (ret) {
		LOG_ERR("Call `task_battery` failed: %d", ret);
		return ret;
	}

	ret = task_chester_z();
	if (ret) {
		LOG_ERR("Call `task_chester_z` failed: %d", ret);
		return ret;
	}

	ret = task_sensors();
	if (ret) {
		LOG_ERR("Call `task_sensors` failed: %d", ret);
		return ret;
	}

	if (atomic_clear(&m_send)) {
		ret = send();
		if (ret) {
			LOG_ERR("Call `send` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int m_wdt_id;

static void init_wdt(void)
{
	int ret;

	if (!device_is_ready(m_wdt_dev)) {
		LOG_ERR("Device `WDT` not ready");
		k_oops();
	}

	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0U,
		.window.max = 120000,
	};

	ret = wdt_install_timeout(m_wdt_dev, &wdt_config);

	if (ret < 0) {
		LOG_ERR("Call `wdt_install_timeout` failed: %d", ret);
		k_oops();
	}

	m_wdt_id = ret;

	ret = wdt_setup(m_wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (ret < 0) {
		LOG_ERR("Call `wdt_setup` failed: %d", ret);
		k_oops();
	}
}

static void feed_wdt(void)
{
	if (!device_is_ready(m_wdt_dev)) {
		LOG_ERR("Device `WDT` not ready");
		k_oops();
	}

	wdt_feed(m_wdt_dev, m_wdt_id);
}

void main(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	init_wdt();

	ret = init_chester_z();
	if (ret) {
		LOG_ERR("Call `init_chester_z` failed: %d", ret);
		k_oops();
	}

	ret = init_sensors();
	if (ret) {
		LOG_ERR("Call `init_sensors` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_init(lrw_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		k_oops();
	}

	k_sleep(K_SECONDS(2));

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	for (;;) {
		LOG_INF("Alive");

		feed_wdt();

		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(30));
		ctr_led_set(CTR_LED_CHANNEL_G, false);

		ret = k_sem_take(&m_loop_sem, K_SECONDS(5));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_oops();
		}

		ret = loop();
		if (ret) {
			LOG_ERR("Call `loop` failed: %d", ret);
			k_oops();
		}
	}
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = send();
	if (ret) {
		LOG_ERR("Call `send` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
