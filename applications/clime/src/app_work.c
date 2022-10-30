#include "app_config.h"
#include "app_init.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_work.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>
#include <zephyr/zephyr.h>

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

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.interval_report * 1000 / 5);
	int64_t duration = g_app_config.interval_report * 1000 + jitter;

	LOG_INF("Scheduling next timeout in %lld second(s)", duration / 1000);

	k_timer_start(&m_send_timer, K_MSEC(duration), K_FOREVER);

	ret = app_send();
	if (ret) {
		LOG_ERR("Call `app_send` failed: %d", ret);
	}

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay)
	app_sensor_iaq_clear();
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay) */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(sht30_ext), okay)
	app_sensor_hygro_clear();
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(sht30_ext), okay) */

#if DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20)
	app_sensor_w1_therm_clear();
#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20) */
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

	ret = app_sensor_sample();
	if (ret < 0) {
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

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay)

static void iaq_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_iaq_sample();
	if (ret < 0) {
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

	ret = app_sensor_iaq_aggregate();
	if (ret < 0) {
		LOG_ERR("Call `app_sensor_iaq_aggregate` failed: %d", ret);
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

#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay) */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(sht30_ext), okay)

static void hygro_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_hygro_sample();
	if (ret < 0) {
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

	ret = app_sensor_hygro_aggregate();
	if (ret < 0) {
		LOG_ERR("Call `app_sensor_hygro_aggregate` failed: %d", ret);
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

#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(sht30_ext), okay) */

#if DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20)

static void w1_therm_sample_work_handler(struct k_work *work)
{
	int ret;

	ret = app_sensor_w1_therm_sample();
	if (ret < 0) {
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

	ret = app_sensor_w1_therm_aggregate();
	if (ret < 0) {
		LOG_ERR("Call `app_sensor_w1_therm_aggregate` failed: %d", ret);
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

#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20) */

int app_work_init(void)
{
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
	                   WORK_Q_PRIORITY, NULL);

	/* TODO Fix name */
	k_thread_name_set(&m_work_q.thread, "APP_WORK Thread");

	k_timer_start(&m_send_timer, K_NO_WAIT, K_FOREVER);

	k_timer_start(&m_sample_timer, K_SECONDS(g_app_config.interval_sample),
	              K_SECONDS(g_app_config.interval_sample));

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay)
	k_timer_start(&m_iaq_sample_timer, K_SECONDS(g_app_config.interval_sample),
	              K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_iaq_aggreg_timer, K_SECONDS(g_app_config.interval_aggregate),
	              K_SECONDS(g_app_config.interval_aggregate));
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay) */

#if DT_NODE_HAS_STATUS(DT_CHOSEN(ctr_hygro), okay)
	k_timer_start(&m_hygro_sample_timer, K_SECONDS(g_app_config.interval_sample),
	              K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_hygro_aggreg_timer, K_SECONDS(g_app_config.interval_aggregate),
	              K_SECONDS(g_app_config.interval_aggregate));
#endif /* DT_NODE_HAS_STATUS(DT_CHOSEN(ctr_hygro), okay) */

#if DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20)
	k_timer_start(&m_w1_therm_sample_timer, K_SECONDS(g_app_config.interval_sample),
	              K_SECONDS(g_app_config.interval_sample));
	k_timer_start(&m_w1_therm_aggreg_timer, K_SECONDS(g_app_config.interval_aggregate),
	              K_SECONDS(g_app_config.interval_aggregate));
#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20) */

	return 0;
}

void app_work_sample(void)
{
	k_timer_start(&m_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay)
	k_timer_start(&m_iaq_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_s1), okay) */

#if DT_NODE_HAS_STATUS(DT_CHOSEN(ctr_hygro), okay)
	k_timer_start(&m_hygro_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* DT_NODE_HAS_STATUS(DT_CHOSEN(ctr_hygro), okay) */

#if DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20)
	k_timer_start(&m_w1_therm_sample_timer, K_NO_WAIT, K_SECONDS(g_app_config.interval_sample));
#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_ds18b20) */
}

void app_work_send(void)
{
	k_timer_start(&m_send_timer, K_NO_WAIT, K_FOREVER);
}
