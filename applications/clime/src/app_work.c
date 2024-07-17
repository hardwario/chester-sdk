/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_backup.h"
#include "app_config.h"
#include "app_cbor.h"
#include "app_init.h"
#include "app_power.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_tamper.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <math.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

static struct k_timer m_send_timer;

static void send_work_handler(struct k_work *work)
{
	int ret;

#if CONFIG_APP_REPORT_JITTER
	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.interval_report * 1000 / 5);
#else
	int64_t jitter = 0;
#endif /* CONFIG_APP_REPORT_JITTER */

	int64_t duration = g_app_config.interval_report * 1000 + jitter;

	LOG_INF("Scheduling next timeout in %lld second(s)", duration / 1000);

	k_timer_start(&m_send_timer, K_MSEC(duration), K_FOREVER);

#if defined(CONFIG_SHIELD_CTR_LTE_V2)

	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		CTR_BUF_DEFINE_STATIC(buf, 8 * 1024);

		ctr_buf_reset(&buf);

		ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

		ret = app_cbor_encode(zs);
		if (ret) {
			LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
			return;
		}

		size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

		ret = ctr_buf_seek(&buf, len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return;
		}

		ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
		if (ret) {
			LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
			return;
		}
	}

#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */

#if defined(CONFIG_SHIELD_CTR_LRW)

	if (g_app_config.mode == APP_CONFIG_MODE_LRW) {

		ret = app_send();
		if (ret) {
			LOG_ERR("Call `app_send` failed: %d", ret);
		}
	}

#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_APP_TAMPER)
	app_tamper_clear();
#endif /* defined(CONFIG_APP_TAMPER) */

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
	app_backup_clear();
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

#if defined(CONFIG_SHIELD_CTR_S1)
	app_sensor_iaq_clear();
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	app_sensor_hygro_clear();
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	app_sensor_w1_therm_clear();
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
	app_sensor_rtd_therm_clear();
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
	app_sensor_soil_sensor_clear();
#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	app_sensor_ble_tag_clear();
#endif /* defined(CONFIG_CTR_BLE_TAG) */
}

static K_WORK_DEFINE(m_send_work, send_work_handler);

static void send_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_send_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_send_timer, send_timer_handler, NULL);

static void sample_work_handler(struct k_work *work)
{
	int ret;

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
	ret = app_backup_sample();
	if (ret) {
		LOG_ERR("Call `app_backup_sample` failed: %d", ret);
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

	ret = app_sensor_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_sample_work, sample_work_handler);

static void sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_sample_timer, sample_timer_handler, NULL);

static void power_work_handler(struct k_work *work)
{
	int ret;

	ret = app_power_sample();
	if (ret) {
		LOG_ERR("Call `app_power_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_power_work, power_work_handler);

static void power_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_power_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_power_timer, power_timer_handler, NULL);

#if defined(CONFIG_SHIELD_CTR_S1)

static void iaq_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_iaq_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_iaq_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_iaq_sample_work, iaq_sample_work_handler);

static void iaq_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_iaq_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_iaq_sample_timer, iaq_sample_timer_handler, NULL);

static void iaq_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_iaq_sensors_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_iaq_sensors_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_iaq_aggreg_work, iaq_aggreg_work_handler);

static void iaq_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_iaq_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_iaq_aggreg_timer, iaq_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)

static void hygro_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_hygro_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_hygro_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_hygro_sample_work, hygro_sample_work_handler);

static void hygro_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_hygro_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_hygro_sample_timer, hygro_sample_timer_handler, NULL);

static void hygro_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_hygro_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_hygro_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_hygro_aggreg_work, hygro_aggreg_work_handler);

static void hygro_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_hygro_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_hygro_aggreg_timer, hygro_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

static void w1_therm_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_w1_therm_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_w1_therm_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_w1_therm_sample_work, w1_therm_sample_work_handler);

static void w1_therm_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_w1_therm_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_w1_therm_sample_timer, w1_therm_sample_timer_handler, NULL);

static void w1_therm_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_w1_therm_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_w1_therm_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_w1_therm_aggreg_work, w1_therm_aggreg_work_handler);

static void w1_therm_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_w1_therm_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_w1_therm_aggreg_timer, w1_therm_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)

static void rtd_therm_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_rtd_therm_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_rtd_therm_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_rtd_therm_sample_work, rtd_therm_sample_work_handler);

static void rtd_therm_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_rtd_therm_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_rtd_therm_sample_timer, rtd_therm_sample_timer_handler, NULL);

static void rtd_therm_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_rtd_therm_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_rtd_therm_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_rtd_therm_aggreg_work, rtd_therm_aggreg_work_handler);

static void rtd_therm_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_rtd_therm_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_rtd_therm_aggreg_timer, rtd_therm_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)

static void soil_sensor_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_soil_sensor_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_soil_sensor_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_soil_sensor_sample_work, soil_sensor_sample_work_handler);

static void soil_sensor_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_soil_sensor_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_soil_sensor_sample_timer, soil_sensor_sample_timer_handler, NULL);

static void soil_sensor_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_soil_sensor_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_soil_sensor_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_soil_sensor_aggreg_work, soil_sensor_aggreg_work_handler);

static void soil_sensor_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_soil_sensor_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_soil_sensor_aggreg_timer, soil_sensor_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

#if defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)

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

#endif /* defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10) */

#if defined(CONFIG_CTR_BLE_TAG)

static void ble_tag_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_ble_tag_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_sample` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_ble_tag_sample_work, ble_tag_sample_work_handler);

static void ble_tag_sample_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_ble_tag_sample_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_ble_tag_sample_timer, ble_tag_sample_timer_handler, NULL);

static void ble_tag_aggreg_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_ble_tag_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_ble_tag_aggreg` failed: %d", ret);
	}
}

static K_WORK_DEFINE(m_ble_tag_aggreg_work, ble_tag_aggreg_work_handler);

static void ble_tag_aggreg_timer_handler(struct k_timer *timer)
{
	int ret;

	ret = k_work_submit_to_queue(&m_work_q, &m_ble_tag_aggreg_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
	}
}

static K_TIMER_DEFINE(m_ble_tag_aggreg_timer, ble_tag_aggreg_timer_handler, NULL);

#endif /* defined(CONFIG_CTR_BLE_TAG) */

int app_work_init(void)
{
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	k_thread_name_set(&m_work_q.thread, "app_work");

	k_timer_start(&m_send_timer, K_SECONDS(10), K_FOREVER);
	k_timer_start(&m_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_power_timer, K_SECONDS(60), K_HOURS(12));

#if defined(CONFIG_SHIELD_CTR_S1)
	k_timer_start(&m_iaq_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_iaq_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	k_timer_start(&m_hygro_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_hygro_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	k_timer_start(&m_w1_therm_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_w1_therm_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
	k_timer_start(&m_rtd_therm_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_rtd_therm_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
	k_timer_start(&m_soil_sensor_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_soil_sensor_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

#if defined(CONFIG_CTR_BLE_TAG)
	k_timer_start(&m_ble_tag_sample_timer, K_SECONDS(g_app_config.interval_sample),
		      K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_ble_tag_aggreg_timer, K_SECONDS(g_app_config.interval_aggreg),
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_CTR_BLE_TAG) */

	return 0;
}

void app_work_sample(void)
{
	k_timer_start(&m_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));

#if defined(CONFIG_SHIELD_CTR_S1)
	k_timer_start(&m_iaq_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	k_timer_start(&m_hygro_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	k_timer_start(&m_w1_therm_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
	k_timer_start(&m_rtd_therm_sample_timer, K_NO_WAIT,
		      K_SECONDS(g_app_config.interval_sample));
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
	k_timer_start(&m_soil_sensor_sample_timer, K_NO_WAIT,
		      K_SECONDS(g_app_config.interval_sample));
#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

	k_timer_start(&m_ble_tag_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
}

void app_work_aggreg(void)
{

#if defined(CONFIG_SHIELD_CTR_S1)
	k_timer_start(&m_iaq_aggreg_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
	k_timer_start(&m_hygro_aggreg_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	k_timer_start(&m_w1_therm_aggreg_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
	k_timer_start(&m_rtd_therm_aggreg_timer, K_NO_WAIT,
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

#if defined(CONFIG_SHIELD_CTR_SOIL_SENSOR)
	k_timer_start(&m_soil_sensor_aggreg_timer, K_NO_WAIT,
		      K_SECONDS(g_app_config.interval_aggreg));
#endif /* defined(CONFIG_SHIELD_CTR_SOIL_SENSOR) */

	k_timer_start(&m_ble_tag_aggreg_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_aggreg));
}

void app_work_send(void)
{
	k_timer_start(&m_send_timer, K_NO_WAIT, K_FOREVER);
}

#if defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) || defined(CONFIG_SHIELD_CTR_X10)
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

#endif /* defined(CONFIG_SHIELD_CTR_S2) || defined(CONFIG_SHIELD_CTR_Z) ||                         \
	  defined(CONFIG_SHIELD_CTR_X10) */
