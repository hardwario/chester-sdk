/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_machine_probe.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define READ_THERMOMETER   0
#define READ_HYGROMETER    1
#define READ_LUX_METER     1
#define READ_MAGNETOMETER  1
#define READ_ACCELEROMETER 1

#if READ_ACCELEROMETER

#define TILT_THRESHOLD 32
#define TILT_DURATION  2

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

static void poll_work_handler(struct k_work *work)
{
	int ret;

	int count = ctr_machine_probe_get_count();

	for (int i = 0; i < count; i++) {
		uint64_t serial_number;

		bool is_active;
		ret = ctr_machine_probe_get_tilt_alert(i, &serial_number, &is_active);
		if (ret) {
			LOG_ERR("Call `ctr_machine_probe_get_tilt_alert` (%d) failed: %d", i, ret);
			continue;
		}

		if (is_active) {
			LOG_INF("Serial number: %llu / Tilt: Alert detected", serial_number);
		}
	}
}

static K_WORK_DEFINE(m_poll_work, poll_work_handler);

static void poll_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_poll_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_poll_timer, poll_timer_handler, NULL);

#endif /* READ_ACCELEROMETER */

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_machine_probe_scan();
	if (ret) {
		LOG_ERR("Call `ctr_machine_probe_scan` failed: %d", ret);
		k_oops();
	}

	int count = ctr_machine_probe_get_count();

#if READ_ACCELEROMETER

	for (int i = 0; i < count; i++) {
		uint64_t serial_number;
		ret = ctr_machine_probe_enable_tilt_alert(i, &serial_number, TILT_THRESHOLD,
							  TILT_DURATION);

		if (ret) {
			LOG_ERR("Call `ctr_machine_probe_enable_tilt_alert` (%d) failed: %d", ret,
				i);
			continue;
		}
	}

	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	k_thread_name_set(&m_work_q.thread, "appworkq");

	k_timer_start(&m_poll_timer, K_NO_WAIT, K_SECONDS(1));

#endif /* READ_ACCELEROMETER */

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		for (int i = 0; i < count; i++) {
			uint64_t serial_number;

#if READ_THERMOMETER
			float thermometer_temperature;
			ret = ctr_machine_probe_read_thermometer(i, &serial_number, &temperature);
			if (ret) {
				LOG_ERR("Call `ctr_machine_probe_read_thermometer` failed: "
					"%d",
					ret);
			} else {
				LOG_INF("Serial number: %llu / Thermometer / Temperature: "
					"%.2f C",
					serial_number, temperature);
			}
#endif /* READ_THERMOMETER */

#if READ_HYGROMETER
			float hygrometer_temperature;
			float hygrometer_humidity;
			ret = ctr_machine_probe_read_hygrometer(
				i, &serial_number, &hygrometer_temperature, &hygrometer_humidity);
			if (ret) {
				LOG_ERR("Call `ctr_machine_probe_read_hygrometer` failed: "
					"%d",
					ret);
			} else {
				LOG_INF("Serial number: %llu / Hygrometer / Temperature: "
					"%.2f C",
					serial_number, hygrometer_temperature);
				LOG_INF("Serial number: %llu / Hygrometer / Humidity: %.1f "
					"%%",
					serial_number, hygrometer_humidity);
			}
#endif /* READ_HYGROMETER */

#if READ_LUX_METER
			float lux_meter_illuminance;
			ret = ctr_machine_probe_read_lux_meter(i, &serial_number,
							       &lux_meter_illuminance);
			if (ret) {
				LOG_ERR("Call `ctr_machine_probe_read_lux_meter` failed: "
					"%d",
					ret);
			} else {
				LOG_INF("Serial number: %llu / Lux meter / Illuminance: "
					"%.0f lux",
					serial_number, lux_meter_illuminance);
			}
#endif /* READ_LUX_METER */

#if READ_MAGNETOMETER
			float megnetometer_magnetic_field;
			ret = ctr_machine_probe_read_magnetometer(i, &serial_number,
								  &megnetometer_magnetic_field);
			if (ret) {
				LOG_ERR("Call `ctr_machine_probe_read_magnetometer` "
					"failed: %d",
					ret);
			} else {
				LOG_INF("Serial number: %llu / Magnetometer / Magnetic "
					"field: %.2f "
					"mT",
					serial_number, megnetometer_magnetic_field);
			}
#endif /* READ_MAGNETOMETER */

#if READ_ACCELEROMETER
			float accelerometer_accel_x;
			float accelerometer_accel_y;
			float accelerometer_accel_z;
			ret = ctr_machine_probe_read_accelerometer(
				i, &serial_number, &accelerometer_accel_x, &accelerometer_accel_y,
				&accelerometer_accel_z, NULL);
			if (ret) {
				LOG_ERR("Call `ctr_machine_probe_read_accelerometer` "
					"failed: %d",
					ret);
			} else {
				LOG_INF("Serial number: %llu / Accelerometer / "
					"Acceleration X: "
					"%.3f m/s^2",
					serial_number, accelerometer_accel_x);
				LOG_INF("Serial number: %llu / Accelerometer / "
					"Acceleration Y: "
					"%.3f m/s^2",
					serial_number, accelerometer_accel_y);
				LOG_INF("Serial number: %llu / Accelerometer / "
					"Acceleration Z: "
					"%.3f m/s^2",
					serial_number, accelerometer_accel_z);
			}
#endif /* READ_ACCELEROMETER */
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
