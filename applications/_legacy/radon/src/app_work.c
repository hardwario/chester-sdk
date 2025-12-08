#include "app_backup.h"
#include "app_config.h"
#include "app_power.h"
#include "app_sensor.h"
#include "app_tamper.h"
#include "app_cbor.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

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

	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_MSEC(duration));

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

#if defined(FEATURE_SUBSYSTEM_CLOUD)
	ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
	if (ret) {
		LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
		return;
	}
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */

	app_sensor_radon_clear();

#if defined(FEATURE_TAMPER)
	app_tamper_clear();
#endif /* defined(FEATURE_TAMPER) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	app_backup_clear();
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	app_sensor_ble_tag_clear();
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */
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

static void radon_sample_work_handler(struct k_work *work)
{
	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_sample));

	int ret = app_sensor_radon_sample();
	if (ret) {
		LOG_ERR("Call `app_sensor_radon_sample` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_radon_sample_work, radon_sample_work_handler);

static void radon_aggreg_work_handler(struct k_work *work)
{
	k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
				  K_SECONDS(g_app_config.interval_aggreg));

	int ret = app_sensor_radon_aggreg();
	if (ret) {
		LOG_ERR("Call `app_sensor_radon_aggreg` failed: %d", ret);
	}
}

static K_WORK_DELAYABLE_DEFINE(m_radon_aggreg_work, radon_aggreg_work_handler);

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

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

#endif /* defined(FEATURE_HARDWARE_BLE_TAG) */

int app_work_init(void)
{
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);
	k_thread_name_set(&m_work_q.thread, "app_work");

	k_work_schedule_for_queue(&m_work_q, &m_send_work, K_SECONDS(10));
	k_work_schedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
	k_work_schedule_for_queue(&m_work_q, &m_power_work, K_SECONDS(60));

	k_work_schedule_for_queue(&m_work_q, &m_radon_sample_work, K_NO_WAIT);
	k_work_schedule_for_queue(&m_work_q, &m_radon_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));

	k_work_schedule_for_queue(&m_work_q, &m_ble_tag_sample_work, K_NO_WAIT);
	k_work_schedule_for_queue(&m_work_q, &m_ble_tag_aggreg_work,
				  K_SECONDS(g_app_config.interval_aggreg));

	return 0;
}

void app_work_sample(void)
{
	k_work_reschedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
	k_work_reschedule_for_queue(&m_work_q, &m_radon_sample_work, K_NO_WAIT);
	k_work_reschedule_for_queue(&m_work_q, &m_ble_tag_sample_work, K_NO_WAIT);
}

void app_work_aggreg(void)
{
	k_work_reschedule_for_queue(&m_work_q, &m_radon_aggreg_work, K_NO_WAIT);
	k_work_reschedule_for_queue(&m_work_q, &m_ble_tag_aggreg_work, K_NO_WAIT);
}

void app_work_send(void)
{
	k_work_reschedule_for_queue(&m_work_q, &m_send_work, K_NO_WAIT);
}

#if defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_TAMPER)
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

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) || defined(FEATURE_TAMPER) */
