/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_init.h"
#include "app_power.h"
#include "app_send.h"
#include "app_sensor.h"
#include "app_work.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

/* Standard includes */
#include <math.h>

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static struct k_work_q m_work_q;

static app_work_sample_cb_t m_sample_cb;
static void *m_sample_cb_user_data;
static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);

static void send_work_handler(struct k_work *work)
{
    int ret;

#if FEATURE_CHESTER_APP_REPORT_JITTER
    int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.interval_report * 1000 / 5);
#else
    int64_t jitter = 0;
#endif /* FEATURE_CHESTER_APP_REPORT_JITTER */

    int64_t duration = g_app_config.interval_report * 1000 + jitter;

    LOG_INF("Scheduling next timeout in %lld second(s)", duration / 1000);

    k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work, K_MSEC(duration));

    ret = app_send();
    if (ret) {
        LOG_ERR("Call `app_send` failed: %d", ret);
    }

#if defined(CONFIG_CTR_S3)
    app_data_lock();

    g_app_data.motion_count_left = 0;
    g_app_data.motion_count_right = 0;

    app_data_unlock();
#endif /* defined(CONFIG_CTR_S3) */
}

static K_WORK_DELAYABLE_DEFINE(m_send_work, send_work_handler);

static void sample_work_handler(struct k_work *work)
{
    int ret;

    k_work_schedule_for_queue(&m_work_q, (struct k_work_delayable *)work,
                 K_SECONDS(g_app_config.interval_sample));

    ret = app_sensor_sample();
    if (ret) {
        LOG_ERR("Call `app_sensor_sample` failed: %d", ret);
    }

    if (m_sample_cb) {
        m_sample_cb(m_sample_cb_user_data);
        m_sample_cb = NULL;
        m_sample_cb_user_data = NULL;
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

int app_work_init(void)
{
    k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
               WORK_Q_PRIORITY, NULL);

    k_thread_name_set(&m_work_q.thread, "app_work");

    k_work_schedule_for_queue(&m_work_q, &m_send_work, K_SECONDS(10));
    k_work_schedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
    k_work_schedule_for_queue(&m_work_q, &m_power_work, K_SECONDS(60));

    return 0;
}

void app_work_sample(void)
{
    k_work_reschedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
}

void app_work_send(void)
{
    k_work_reschedule_for_queue(&m_work_q, &m_send_work, K_NO_WAIT);
}

void app_work_sample_with_cb(app_work_sample_cb_t cb, void *user_data)
{
    m_sample_cb = cb;
    m_sample_cb_user_data = user_data;
    k_work_reschedule_for_queue(&m_work_q, &m_sample_work, K_NO_WAIT);
}

