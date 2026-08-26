/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_led.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_z.h>
#include <chester/drivers/ctr_s3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

extern struct app_config g_app_config;
#if defined(FEATURE_SUBSYSTEM_BUTTON)

static void app_load_timer_handler(struct k_timer *timer)
{
    ctr_led_set(CTR_LED_CHANNEL_LOAD, 0);
}

K_TIMER_DEFINE(app_load_timer, app_load_timer_handler, NULL);

void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
              void *user_data)
{
    int ret;

    if (chan != CTR_BUTTON_CHANNEL_INT) {
       return;
    }

    if (ev == CTR_BUTTON_EVENT_CLICK) {
       for (int i = 0; i < val; i++) {
          ret = ctr_led_set(CTR_LED_CHANNEL_Y, true);
          if (ret) {
             LOG_ERR("Call `ctr_led_set` failed: %d", ret);
             return;
          }

          k_sleep(K_MSEC(50));
          ret = ctr_led_set(CTR_LED_CHANNEL_Y, false);
          if (ret) {
             LOG_ERR("Call `ctr_led_set` failed: %d", ret);
             return;
          }

          k_sleep(K_MSEC(200));
       }

       switch (val) {
       case 1:
          app_work_send();
          break;
       case 2:
          app_work_sample();
          break;
       case 3:
          app_work_sample();
          app_work_send();
          break;
       case 4:
          sys_reboot(SYS_REBOOT_COLD);
          break;
       case 5:
          ret = ctr_led_set(CTR_LED_CHANNEL_LOAD, 1);
          if (ret) {
             LOG_ERR("Call `ctr_led_set` failed: %d", ret);
             return;
          }
          k_timer_start(&app_load_timer, K_MINUTES(2), K_FOREVER);
          break;
       }
    }
}

#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(CONFIG_CTR_S3)

static enum {
   STATE_IDLE,
   STATE_L_DETECTED,
   STATE_R_DETECTED,
} motion_state = STATE_IDLE;

static int64_t m_first_detection_uptime;
static app_motion_detection_cb_t m_detection_cb;
static void *m_detection_cb_user_data;

static void reset_motion_state(struct k_timer *timer)
{
   motion_state = STATE_IDLE;
   LOG_DBG("Motion state reset");
}

K_TIMER_DEFINE(motion_state_timer, reset_motion_state, NULL);

void app_handler_set_detection_cb(app_motion_detection_cb_t cb, void *user_data)
{
   m_detection_cb = cb;
   m_detection_cb_user_data = user_data;
}

void app_handler_clear_detection_cb(void)
{
   m_detection_cb = NULL;
   m_detection_cb_user_data = NULL;
}

/* Record a completed passage and notify the detection callback. Must be called
 * with the app data lock held.
 */
static void record_passage(enum app_data_direction direction)
{
	int64_t delta_ms = k_uptime_get() - m_first_detection_uptime;

	if (delta_ms < 0 || delta_ms > UINT16_MAX) {
		LOG_WRN("Passage delta out of range: %lld ms", delta_ms);
		return;
	}

	if (g_app_data.passage_event_count >= APP_DATA_MAX_PASSAGE_EVENTS) {
		LOG_WRN("Passage buffer full, dropping event");
	} else {
		int64_t timestamp = 0;
		int ret = ctr_rtc_get_ts(&timestamp);

		if (ret) {
			LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
		}

		int idx = g_app_data.passage_event_count;

		g_app_data.passage_events[idx].timestamp = timestamp;
		g_app_data.passage_events[idx].direction = direction;
		g_app_data.passage_events[idx].delta_ms = (uint16_t)delta_ms;
		g_app_data.passage_event_count = idx + 1;
	}

	if (m_detection_cb) {
		struct app_detection_event ev = {
			.direction = (int)direction,
			.delta_ms = (int)delta_ms,
			.detect_left = g_app_data.motion_count_l,
			.detect_right = g_app_data.motion_count_r,
			.motion_left = g_app_data.motion_count_left,
			.motion_right = g_app_data.motion_count_right,
		};

		m_detection_cb(&ev, m_detection_cb_user_data);
	}
}

void app_handler_ctr_s3(const struct device *dev, enum ctr_s3_event event, void *user_data)
{
   if (g_app_config.service_mode_enabled) {
      switch (event) {
         case CTR_S3_EVENT_MOTION_L_DETECTED:
            ctr_led_set(CTR_LED_CHANNEL_G, true);
            k_sleep(K_MSEC(100));
            ctr_led_set(CTR_LED_CHANNEL_G, false);
            break;
         case CTR_S3_EVENT_MOTION_R_DETECTED:
            ctr_led_set(CTR_LED_CHANNEL_R, true);
            k_sleep(K_MSEC(100));
            ctr_led_set(CTR_LED_CHANNEL_R, false);
            break;
         default:
            break;
      }
   }
   switch (event) {
      case CTR_S3_EVENT_MOTION_L_DETECTED:
         LOG_INF("Motion L detected");
         app_data_lock();

         if (motion_state == STATE_IDLE) {
            motion_state = STATE_L_DETECTED;
            m_first_detection_uptime = k_uptime_get();
            g_app_data.motion_count_l++;
            g_app_data.total_detect_left++;
            k_timer_start(&motion_state_timer, K_MSEC(750), K_NO_WAIT);
         } else if (motion_state == STATE_R_DETECTED) {
            g_app_data.motion_count_l++;
            g_app_data.total_detect_left++;
            LOG_INF("Movement detected from right to left.");
            g_app_data.last_motion_direction = APP_DATA_DIRECTION_RIGHT_TO_LEFT;
            g_app_data.motion_count_left++;
            g_app_data.total_motion_left++;
            k_timer_stop(&motion_state_timer);
            motion_state = STATE_IDLE;

            record_passage(APP_DATA_DIRECTION_RIGHT_TO_LEFT);
         }

         app_data_unlock();
         break;

      case CTR_S3_EVENT_MOTION_R_DETECTED:
         LOG_INF("Motion R detected");
         app_data_lock();

         if (motion_state == STATE_IDLE) {
            motion_state = STATE_R_DETECTED;
            m_first_detection_uptime = k_uptime_get();
            g_app_data.motion_count_r++;
            g_app_data.total_detect_right++;
            k_timer_start(&motion_state_timer, K_MSEC(750), K_NO_WAIT);
         } else if (motion_state == STATE_L_DETECTED) {
            g_app_data.motion_count_r++;
            g_app_data.total_detect_right++;
            LOG_INF("Movement detected from left to right.");
            g_app_data.last_motion_direction = APP_DATA_DIRECTION_LEFT_TO_RIGHT;
            g_app_data.motion_count_right++;
            g_app_data.total_motion_right++;
            k_timer_stop(&motion_state_timer);
            motion_state = STATE_IDLE;

            record_passage(APP_DATA_DIRECTION_LEFT_TO_RIGHT);
         }

         app_data_unlock();
         break;

      default:
         LOG_ERR("Unknown event: %d", event);
         return;
   }
}

#endif /* defined(CONFIG_CTR_S3) */
