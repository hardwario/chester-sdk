/* TODO Refactor inputs to work with arrays (this was a dirty copy/paste for time reasons :)) */

/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_batt.h>
#include <ctr_bsp.h>
#include <ctr_buf.h>
#include <ctr_chester_x0d.h>
#include <ctr_hygro.h>
#include <ctr_led.h>
#include <ctr_lrw.h>
#include <ctr_therm.h>
#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <logging/log.h>
#include <random/rand32.h>
#include <shell/shell.h>
#include <sys/reboot.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* TODO Would be nice to define using K_SECONDS, etc. Proper macros? */
#define BATT_TEST_INTERVAL_MSEC (6 * 60 * 60 * 1000)
#define REPORT_INTERVAL_MSEC (15 * 60 * 1000)

#if IS_ENABLED(CONFIG_APP_Z)
static const struct device *m_ctr_z_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));
#endif /* IS_ENABLED(CONFIG_APP_Z) */

struct data_errors {
	bool orientation;
	bool int_temperature;
	bool ext_temperature;
	bool ext_humidity;
	bool input_1;
	bool input_2;
	bool input_3;
	bool input_4;
	bool input_5;
	bool input_6;
	bool input_7;
	bool input_8;
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
	int input_1_level;
	int input_2_level;
	int input_3_level;
	int input_4_level;
	int input_5_level;
	int input_6_level;
	int input_7_level;
	int input_8_level;
	bool line_present;
	float line_voltage;
	float bckp_voltage;
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
};

struct data_events {
	atomic_t device_boot;
	atomic_t device_tilt;
	atomic_t input_1_rise;
	atomic_t input_1_fall;
	atomic_t input_2_rise;
	atomic_t input_2_fall;
	atomic_t input_3_rise;
	atomic_t input_3_fall;
	atomic_t input_4_rise;
	atomic_t input_4_fall;
	atomic_t input_5_rise;
	atomic_t input_5_fall;
	atomic_t input_6_rise;
	atomic_t input_6_fall;
	atomic_t input_7_rise;
	atomic_t input_7_fall;
	atomic_t input_8_rise;
	atomic_t input_8_fall;
};

struct data {
	struct data_errors errors;
	struct data_states states;
	struct data_events events;
};

static struct data m_data = { .errors = {
	                              .orientation = true,
	                              .int_temperature = true,
	                              .ext_temperature = true,
	                              .ext_humidity = true,
	                              .input_1 = true,
	                              .input_2 = true,
	                              .input_3 = true,
	                              .input_4 = true,
	                              .input_5 = true,
	                              .input_6 = true,
	                              .input_7 = true,
	                              .input_8 = true,
	                              .line_present = true,
	                              .line_voltage = true,
	                              .bckp_voltage = true,
	                              .batt_voltage_rest = true,
	                              .batt_voltage_load = true,
	                              .batt_current_load = true,
	                      } };

static K_SEM_DEFINE(m_loop_sem, 1, 1);

static atomic_t m_send;

static int task_battery(void)
{
	int ret;

	static int64_t next;

	if (k_uptime_get() >= next) {
		struct ctr_batt_result result;

		ret = ctr_batt_measure(&result);

		if (ret < 0) {
			LOG_ERR("Call `ctr_batt_measure` failed: %d", ret);
			goto error;
		}

		LOG_INF("Battery voltage (rest): %u mV", result.voltage_rest_mv);
		LOG_INF("Battery voltage (load): %u mV", result.voltage_load_mv);
		LOG_INF("Battery current (load): %u mA", result.current_load_ma);

		next = k_uptime_get() + BATT_TEST_INTERVAL_MSEC;

		m_data.errors.batt_voltage_rest = false;
		m_data.errors.batt_voltage_load = false;
		m_data.errors.batt_current_load = false;

		m_data.states.batt_voltage_rest = result.voltage_rest_mv / 1000.f;
		m_data.states.batt_voltage_load = result.voltage_load_mv / 1000.f;
		m_data.states.batt_current_load = result.current_load_ma;
	}

	return 0;

error:

	m_data.errors.batt_voltage_rest = true;
	m_data.errors.batt_voltage_load = true;
	m_data.errors.batt_current_load = true;

	return ret;
}

#if IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B)
static void chester_x0d_event_handler(enum ctr_chester_x0d_slot slot,
                                      enum ctr_chester_x0d_channel channel,
                                      enum ctr_chester_x0d_event event, void *param)
{
	LOG_DBG("slot: %d channel: %d event: %d", (int)slot, (int)channel, (int)event);

#if IS_ENABLED(CONFIG_APP_X0D_A)
	if (slot == CTR_CHESTER_X0D_SLOT_A && channel == CTR_CHESTER_X0D_CHANNEL_1) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 1 detected rising edge");

			atomic_inc(&m_data.events.input_1_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 1 detected falling edge");

			atomic_inc(&m_data.events.input_1_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_A && channel == CTR_CHESTER_X0D_CHANNEL_2) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 2 detected rising edge");

			atomic_inc(&m_data.events.input_2_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 2 detected falling edge");

			atomic_inc(&m_data.events.input_2_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_A && channel == CTR_CHESTER_X0D_CHANNEL_3) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 3 detected rising edge");

			atomic_inc(&m_data.events.input_3_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 3 detected falling edge");

			atomic_inc(&m_data.events.input_3_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_A && channel == CTR_CHESTER_X0D_CHANNEL_4) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 4 detected rising edge");

			atomic_inc(&m_data.events.input_4_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 4 detected falling edge");

			atomic_inc(&m_data.events.input_4_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) */

#if IS_ENABLED(CONFIG_APP_X0D_B)
	if (slot == CTR_CHESTER_X0D_SLOT_B && channel == CTR_CHESTER_X0D_CHANNEL_1) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 5 detected rising edge");

			atomic_inc(&m_data.events.input_5_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 5 detected falling edge");

			atomic_inc(&m_data.events.input_5_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_B && channel == CTR_CHESTER_X0D_CHANNEL_2) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 6 detected rising edge");

			atomic_inc(&m_data.events.input_6_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 6 detected falling edge");

			atomic_inc(&m_data.events.input_6_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_B && channel == CTR_CHESTER_X0D_CHANNEL_3) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 7 detected rising edge");

			atomic_inc(&m_data.events.input_7_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 7 detected falling edge");

			atomic_inc(&m_data.events.input_7_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}

	if (slot == CTR_CHESTER_X0D_SLOT_B && channel == CTR_CHESTER_X0D_CHANNEL_4) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 8 detected rising edge");

			atomic_inc(&m_data.events.input_8_rise);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 8 detected falling edge");

			atomic_inc(&m_data.events.input_8_fall);
			atomic_set(&m_send, true);
			k_sem_give(&m_loop_sem);
		}
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_B) */
}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B) */

#if IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B)
static int init_chester_x0d(void)
{
	int ret;

#if IS_ENABLED(CONFIG_APP_X0D_A)
	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_1,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_2,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_3,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_4,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) */

#if IS_ENABLED(CONFIG_APP_X0D_B)
	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_1,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_2,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_3,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_4,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		return ret;
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_B) */

	return 0;
}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B) */

#if IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B)
static int task_chester_x0d(void)
{
	int ret;

	bool error = false;

#if IS_ENABLED(CONFIG_APP_X0D_A)
	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_1,
	                           &m_data.states.input_1_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_1 = true;
		error = true;
	} else {
		LOG_INF("Channel 1 level: %d", m_data.states.input_1_level);
		m_data.errors.input_1 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_2,
	                           &m_data.states.input_2_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_2 = true;
		error = true;
	} else {
		LOG_INF("Channel 2 level: %d", m_data.states.input_2_level);
		m_data.errors.input_2 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_3,
	                           &m_data.states.input_3_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_3 = true;
		error = true;
	} else {
		LOG_INF("Channel 3 level: %d", m_data.states.input_3_level);
		m_data.errors.input_3 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_4,
	                           &m_data.states.input_4_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_4 = true;
		error = true;
	} else {
		LOG_INF("Channel 4 level: %d", m_data.states.input_4_level);
		m_data.errors.input_4 = false;
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) */

#if IS_ENABLED(CONFIG_APP_X0D_B)
	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_1,
	                           &m_data.states.input_5_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_5 = true;
		error = true;
	} else {
		LOG_INF("Channel 5 level: %d", m_data.states.input_5_level);
		m_data.errors.input_5 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_2,
	                           &m_data.states.input_6_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_6 = true;
		error = true;
	} else {
		LOG_INF("Channel 6 level: %d", m_data.states.input_6_level);
		m_data.errors.input_6 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_3,
	                           &m_data.states.input_7_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_7 = true;
		error = true;
	} else {
		LOG_INF("Channel 7 level: %d", m_data.states.input_7_level);
		m_data.errors.input_7 = false;
	}

	ret = ctr_chester_x0d_read(CTR_CHESTER_X0D_SLOT_B, CTR_CHESTER_X0D_CHANNEL_4,
	                           &m_data.states.input_8_level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		m_data.errors.input_8 = true;
		error = true;
	} else {
		LOG_INF("Channel 8 level: %d", m_data.states.input_8_level);
		m_data.errors.input_8 = false;
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_B) */

	return error ? -EIO : 0;
}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B) */

#if IS_ENABLED(CONFIG_APP_Z)
void ctr_z_event_handler(const struct device *dev, enum ctr_z_event event, void *param)
{
	switch (event) {
	case CTR_Z_EVENT_DC_CONNECTED:
		LOG_INF("Event `CTR_CHESTER_Z_EVENT_DC_CONNECTED`");
		atomic_set(&m_send, true);
		k_sem_give(&m_loop_sem);
		break;

	case CTR_Z_EVENT_DC_DISCONNECTED:
		LOG_INF("Event `CTR_CHESTER_Z_EVENT_DC_DISCONNECTED`");
		atomic_set(&m_send, true);
		k_sem_give(&m_loop_sem);
		break;

	default:
		break;
	}
}
#endif /* IS_ENABLED(CONFIG_APP_Z) */

#if IS_ENABLED(CONFIG_APP_Z)
static int init_chester_z(void)
{
	int ret;

	if (!device_is_ready(m_ctr_z_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_z_set_callback(m_ctr_z_dev, ctr_z_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_set_callback` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;

	ret = ctr_z_get_serial_number(m_ctr_z_dev, &serial_number);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		return ret;
	}

	LOG_INF("Serial number: %08x", serial_number);

	uint16_t hw_revision;

	ret = ctr_z_get_hw_revision(m_ctr_z_dev, &hw_revision);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW revision: %04x", hw_revision);

	uint32_t hw_variant;

	ret = ctr_z_get_hw_variant(m_ctr_z_dev, &hw_variant);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW variant: %08x", hw_variant);

	uint32_t fw_version;

	ret = ctr_z_get_fw_version(m_ctr_z_dev, &fw_version);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		return ret;
	}

	LOG_INF("FW version: %08x", fw_version);

	char *vendor_name;

	ret = ctr_z_get_vendor_name(m_ctr_z_dev, &vendor_name);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_vendor_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Vendor name: %s", vendor_name);

	char *product_name;

	ret = ctr_z_get_product_name(m_ctr_z_dev, &product_name);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_product_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Product name: %s", product_name);

	return 0;
}
#endif /* IS_ENABLED(CONFIG_APP_Z) */

#if IS_ENABLED(CONFIG_APP_Z)
static int task_chester_z(void)
{
	int ret;

	struct ctr_z_status status;

	ret = ctr_z_get_status(m_ctr_z_dev, &status);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
		goto error;
	}

	LOG_INF("DC input connected: %d", (int)status.dc_input_connected);

	uint16_t vdc;

	ret = ctr_z_get_vdc_mv(m_ctr_z_dev, &vdc);

	if (ret < 0) {
		LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage DC input: %u mV", vdc);

	uint16_t vbat;

	ret = ctr_z_get_vbat_mv(m_ctr_z_dev, &vbat);

	if (ret < 0) {
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
#endif /* IS_ENABLED(CONFIG_APP_Z) */

static int init_sensors(void)
{
	int ret;

	ret = ctr_therm_init();

	if (ret < 0) {
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

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		}

		break;

	case CTR_LRW_EVENT_START_OK:
		LOG_INF("Event `CTR_LRW_EVENT_START_OK`");

		ret = ctr_lrw_join(NULL);

		if (ret < 0) {
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
		LOG_WRN("Unknown event: %d", (int)event);
	}
}

static int compose(struct ctr_buf *buf, const struct data *data)
{
	int ret = 0;

	ctr_buf_reset(buf);

	uint32_t flags = 0;

	flags |= data->states.input_1_level != 0 ? 0x00000001 : 0;
	flags |= data->states.input_2_level != 0 ? 0x00000002 : 0;
	flags |= data->states.input_3_level != 0 ? 0x00000004 : 0;
	flags |= data->states.input_4_level != 0 ? 0x00000008 : 0;
	flags |= data->states.input_5_level != 0 ? 0x00000010 : 0;
	flags |= data->states.input_6_level != 0 ? 0x00000020 : 0;
	flags |= data->states.input_7_level != 0 ? 0x00000040 : 0;
	flags |= data->states.input_8_level != 0 ? 0x00000080 : 0;

	flags |= data->errors.input_1 != 0 ? 0x00000100 : 0;
	flags |= data->errors.input_2 != 0 ? 0x00000200 : 0;
	flags |= data->errors.input_3 != 0 ? 0x00000400 : 0;
	flags |= data->errors.input_4 != 0 ? 0x00000800 : 0;
	flags |= data->errors.input_5 != 0 ? 0x00001000 : 0;
	flags |= data->errors.input_6 != 0 ? 0x00002000 : 0;
	flags |= data->errors.input_7 != 0 ? 0x00004000 : 0;
	flags |= data->errors.input_8 != 0 ? 0x00008000 : 0;

	flags |= data->states.line_present ? 0x40000000 : 0;
	flags |= data->errors.line_present ? 0x80000000 : 0;

	ret |= ctr_buf_append_u32(buf, flags);

	if (data->errors.orientation) {
		ret |= ctr_buf_append_u8(buf, 0x00);
	} else {
		ret |= ctr_buf_append_u8(buf, data->states.orientation);
	}

	if (data->errors.int_temperature) {
		ret |= ctr_buf_append_s16(buf, 0x7fff);
	} else {
		int16_t val = data->states.int_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_temperature) {
		ret |= ctr_buf_append_s16(buf, 0x7fff);
	} else {
		int16_t val = data->states.ext_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_humidity) {
		ret |= ctr_buf_append_u8(buf, 0xff);
	} else {
		uint8_t val = data->states.ext_humidity * 2.f;
		ret |= ctr_buf_append_u8(buf, val);
	}

	if (data->errors.line_voltage) {
		ret |= ctr_buf_append_u16(buf, 0xffff);
	} else {
		uint16_t val = data->states.line_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.bckp_voltage) {
		ret |= ctr_buf_append_u16(buf, 0xffff);
	} else {
		uint16_t val = data->states.bckp_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_rest) {
		ret |= ctr_buf_append_u16(buf, 0xffff);
	} else {
		uint16_t val = data->states.batt_voltage_rest * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_load) {
		ret |= ctr_buf_append_u16(buf, 0xffff);
	} else {
		uint16_t val = data->states.batt_voltage_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_current_load) {
		ret |= ctr_buf_append_u16(buf, 0xffff);
	} else {
		uint16_t val = data->states.batt_current_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.device_boot));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.device_tilt));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_1_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_1_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_2_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_2_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_3_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_3_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_4_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_4_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_5_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_5_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_6_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_6_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_7_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_7_fall));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_8_rise));
	ret |= ctr_buf_append_u8(buf, atomic_get(&data->events.input_8_fall));

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

	if (ret < 0) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	struct ctr_lrw_send_opts opts = CTR_LRW_SEND_OPTS_DEFAULTS;

	opts.confirmed = true;
	opts.port = 1;

	ret = ctr_lrw_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void loop(void)
{
	int ret;

	ret = task_battery();

	if (ret < 0) {
		LOG_ERR("Call `task_battery` failed: %d", ret);
	}

#if IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B)
	ret = task_chester_x0d();

	if (ret < 0) {
		LOG_ERR("Call `task_chester_x0d` failed: %d", ret);
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B) */

#if IS_ENABLED(CONFIG_APP_Z)
	ret = task_chester_z();

	if (ret < 0) {
		LOG_ERR("Call `task_chester_z` failed: %d", ret);
	}
#endif /* IS_ENABLED(CONFIG_APP_Z) */

	ret = task_sensors();

	if (ret < 0) {
		LOG_ERR("Call `task_sensors` failed: %d", ret);
	}

	if (atomic_get(&m_send)) {
		atomic_set(&m_send, false);

		ret = send();

		if (ret < 0) {
			LOG_ERR("Call `send` failed: %d", ret);
		}
	}
}

void main(void)
{
	int ret;

	LOG_INF("Application started");

	atomic_inc(&m_data.events.device_boot);

	ctr_bsp_set_led(CTR_BSP_LED_G, true);
	k_sleep(K_MSEC(2000));
	ctr_bsp_set_led(CTR_BSP_LED_G, false);

#if IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B)
	ret = init_chester_x0d();

	if (ret < 0) {
		LOG_ERR("Call `init_chester_x0d` failed: %d", ret);
		k_oops();
	}
#endif /* IS_ENABLED(CONFIG_APP_X0D_A) || IS_ENABLED(CONFIG_APP_X0D_B) */

#if IS_ENABLED(CONFIG_APP_Z)
	ret = init_chester_z();

	if (ret < 0) {
		LOG_ERR("Call `init_chester_z` failed: %d", ret);
		k_oops();
	}
#endif /* IS_ENABLED(CONFIG_APP_Z) */

	ret = init_sensors();

	if (ret < 0) {
		LOG_ERR("Call `init_sensors` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_init(lrw_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_start(NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		k_oops();
	}

	atomic_set(&m_send, true);

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		ret = k_sem_take(&m_loop_sem, K_SECONDS(5));

		if (ret == -EAGAIN) {
			continue;
		}

		if (ret < 0) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_sleep(K_SECONDS(1));
			continue;
		}

		loop();
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

	if (ret < 0) {
		LOG_ERR("Call `send` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
