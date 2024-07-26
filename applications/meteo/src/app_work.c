/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_config.h"
#include "app_init.h"
#include "app_power.h"
#include "app_sensor.h"
#include "app_work.h"
#include "app_cbor.h"
#include "app_send.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>

/* Standard includes */
#include <math.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

static void send_work_handler(struct k_work *work)
{
	int ret;
	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.interval_report * 1000 / 5);
	int64_t duration = g_app_config.interval_report * 1000 + jitter;

	LOG_INF("Scheduling next timeout in %lld second(s)", duration / 1000);

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_MSEC(duration));

	ret = app_send();
	if (ret) {
		LOG_ERR("Call `app_send` failed: %d", ret);
		return;
	}

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	app_backup_clear();
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	app_sensor_meteo_clear();
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	app_sensor_barometer_clear();
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	app_sensor_hygro_clear();
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	app_sensor_w1_therm_clear();
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	app_sensor_soil_sensor_clear();
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	app_sensor_ble_tag_clear();
#endif /* defined(CONFIG_CTR_BLE_TAG) */

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)
	app_sensor_lambrecht_clear();
#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */
}

static K_WORK_DELAYABLE_DEFINE(m_send_work, send_work_handler);

static void sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	ret = app_backup_sample();
	if (ret) {
		LOG_ERR("Call `app_backup_sample` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

	ret = app_sensor_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_sample_work, sample_work_handler);

static void power_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_HOURS(12));

	ret = app_power_sample();
	if (ret) {
		LOG_ERR("Call `app_power_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_power_work, power_work_handler);

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)

static void meteo_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_meteo_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_meteo_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_meteo_sample_work, meteo_sample_work_handler);

static void meteo_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_meteo_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_meteo_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_meteo_aggreg_work, meteo_aggreg_work_handler);

#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)

static void barometer_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_barometer_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_barometer_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_barometer_sample_work, barometer_sample_work_handler);

static void barometer_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_barometer_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_barometer_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_barometer_aggreg_work, barometer_aggreg_work_handler);

#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)

static void hygro_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_hygro_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_hygro_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_hygro_sample_work, hygro_sample_work_handler);

static void hygro_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_hygro_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_hygro_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_hygro_aggreg_work, hygro_aggreg_work_handler);

#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)

static void w1_therm_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_w1_therm_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_w1_therm_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_w1_therm_sample_work, w1_therm_sample_work_handler);

static void w1_therm_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_w1_therm_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_w1_therm_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_w1_therm_aggreg_work, w1_therm_aggreg_work_handler);

#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)

static void soil_sensor_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_soil_sensor_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_soil_sensor_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_soil_sensor_sample_work, soil_sensor_sample_work_handler);

static void soil_sensor_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_soil_sensor_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_soil_sensor_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_soil_sensor_aggreg_work, soil_sensor_aggreg_work_handler);

#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)

static void backup_work_handler(struct k_work *work)
{
	int ret;

	ret = app_backup_sample();
	if (ret) {
		LOG_ERR("Call `app_backup_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_backup_work, backup_work_handler);

void app_work_backup_update(void)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_backup_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(CONFIG_CTR_BLE_TAG)

static void ble_tag_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_ble_tag_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_ble_tag_sample_work, ble_tag_sample_work_handler);

static void ble_tag_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_ble_tag_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_ble_tag_aggreg_work, ble_tag_aggreg_work_handler);

#endif /* defined(CONFIG_CTR_BLE_TAG) */

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)

static void lambrecht_sample_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	ret = app_sensor_lambrecht_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_lambrecht_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_lambrecht_sample_work, lambrecht_sample_work_handler);

static void lambrecht_aggreg_work_handler(struct k_work *work)
{
	int ret;

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	ret = app_sensor_lambrecht_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_lambrecht_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_lambrecht_aggreg_work, lambrecht_aggreg_work_handler);

#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */

int app_work_init(void)
{
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	k_thread_name_set(&m_work_q.thread, "app_work");

	k_work_schedule_for_queue(&m_work_q, &m_send_work, K_SECONDS(10));
	k_work_schedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
	k_work_schedule_for_queue(&m_work_q, &m_power_work, K_SECONDS(60));

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	k_work_schedule_for_queue(&m_work_q, &m_meteo_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_meteo_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	k_work_schedule_for_queue(&m_work_q, &m_barometer_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_barometer_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	k_work_schedule_for_queue(&m_work_q, &m_hygro_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_hygro_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	k_work_schedule_for_queue(&m_work_q, &m_w1_therm_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_w1_therm_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	k_work_schedule_for_queue(&m_work_q, &m_soil_sensor_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_soil_sensor_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	k_work_schedule_for_queue(&m_work_q, &m_ble_tag_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_ble_tag_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_CTR_BLE_TAG) */

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)
	k_work_schedule_for_queue(&m_work_q, &m_lambrecht_sample_work,
				  K_SECONDS(g_app_config.interval_sample));
	k_work_schedule_for_queue(&m_work_q, &m_lambrecht_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */

	return 0;
}

void app_work_aggreg(void)
{
#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	k_work_reschedule_for_queue(&m_work_q, &m_meteo_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	k_work_reschedule_for_queue(&m_work_q, &m_barometer_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	k_work_reschedule_for_queue(&m_work_q, &m_hygro_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	k_work_reschedule_for_queue(&m_work_q, &m_w1_therm_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	k_work_reschedule_for_queue(&m_work_q, &m_soil_sensor_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	k_work_reschedule_for_queue(&m_work_q, &m_soil_sensor_aggreg_work, K_NO_WAIT);
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	k_work_reschedule_for_queue(&m_work_q, &m_ble_tag_aggreg_work, K_NO_WAIT);
#endif /* defined(CONFIG_CTR_BLE_TAG) */
}

void app_work_sample(void)
{
	k_work_reschedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	k_work_reschedule_for_queue(&m_work_q, &m_meteo_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	k_work_reschedule_for_queue(&m_work_q, &m_barometer_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	k_work_reschedule_for_queue(&m_work_q, &m_hygro_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	k_work_reschedule_for_queue(&m_work_q, &m_w1_therm_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	k_work_reschedule_for_queue(&m_work_q, &m_soil_sensor_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	k_work_reschedule_for_queue(&m_work_q, &m_ble_tag_sample_work, K_NO_WAIT);
#endif /* defined(CONFIG_CTR_BLE_TAG) */

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)
	k_work_reschedule_for_queue(&m_work_q, &m_lambrecht_sample_work, K_NO_WAIT);
#endif /* defined(FEATURE_CHESTER_APP_LAMBRECHT) */
}

void app_work_send(void)
{
	k_work_reschedule_for_queue(&m_work_q, &m_send_work, K_NO_WAIT);
}

#if defined(FEATURE_HARDWARE_CHESTER_S2) || defined(FEATURE_HARDWARE_CHESTER_Z) ||                 \
	defined(FEATURE_HARDWARE_CHESTER_X10)
static atomic_t m_report_rate_hourly_counter = 0;
static atomic_t m_report_rate_timer_is_active = false;
static atomic_t m_report_delay_timer_is_active = false;

static void report_delay_timer_handler(struct k_timer *timer)
{
	app_work_send();
	atomic_inc(&m_report_rate_hourly_counter);
	atomic_set(&m_report_delay_timer_is_active, false);
}

static K_TIMER_DEFINE(m_report_delay_timer, report_delay_timer_handler, NULL);

static void report_rate_timer_handler(struct k_timer *timer)
{
	atomic_set(&m_report_rate_hourly_counter, 0);
	atomic_set(&m_report_rate_timer_is_active, false);
}

static K_TIMER_DEFINE(m_report_rate_timer, report_rate_timer_handler, NULL);

void app_work_send_with_rate_limit(void)
{
	if (!atomic_get(&m_report_rate_timer_is_active)) {
		k_timer_start(&m_report_rate_timer, K_HOURS(1), K_NO_WAIT);
		atomic_set(&m_report_rate_timer_is_active, true);
	}

	LOG_INF("Hourly counter state: %d/%d", (int)atomic_get(&m_report_rate_hourly_counter),
		g_app_config.event_report_rate);

	if (atomic_get(&m_report_rate_hourly_counter) <= g_app_config.event_report_rate) {
		if (!atomic_set(&m_report_delay_timer_is_active, true)) {
			LOG_INF("Starting delay timer");
			k_timer_start(&m_report_delay_timer,
				      K_SECONDS(g_app_config.event_report_delay), K_NO_WAIT);
		} else {
			LOG_INF("Delay timer already running");
		}
	} else {
		LOG_WRN("Hourly counter exceeded");
	}
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) || defined(FEATURE_HARDWARE_CHESTER_Z) ||           \
	  defined(FEATURE_HARDWARE_CHESTER_X10) */
