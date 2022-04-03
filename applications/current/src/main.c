#include "app_config.h"

/* CHESTER includes */
#include <ctr_buf.h>
#include <ctr_gpio.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_signal.h>
#include <ctr_rtc.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/watchdog.h>
#include <logging/log.h>
#include <random/rand32.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *m_wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));

#define FULLSCALE_MV 625.0

struct sample {
	int timestamp_offset;
	int avg_milliamps;
	int rms_milliamps;
};

struct data {
	int sample_count;
	int64_t timestamp;
	struct sample samples[128];
};

static struct data m_data;
static K_SEM_DEFINE(m_run_sem, 0, 1);
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static atomic_t m_measure;
static atomic_t m_send = true;

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

static void lte_event_handler(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
{
	ARG_UNUSED(param);

	switch (event) {
	case CTR_LTE_EVENT_FAILURE:
		LOG_ERR("Event `CTR_LTE_EVENT_FAILURE`");
		start();
		break;

	case CTR_LTE_EVENT_START_OK:
		LOG_INF("Event `CTR_LTE_EVENT_START_OK`");
		attach();
		break;

	case CTR_LTE_EVENT_START_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_START_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_ATTACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_ATTACH_OK`");
		k_sem_give(&m_run_sem);
		break;

	case CTR_LTE_EVENT_ATTACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_ATTACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_DETACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_DETACH_OK`");
		break;

	case CTR_LTE_EVENT_DETACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_DETACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LTE_EVENT_SEND_OK`");
		break;

	case CTR_LTE_EVENT_SEND_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_SEND_ERR`");
		start();
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
		return;
	}
}

static int compose(struct ctr_buf *buf, const struct data *data)
{
	int ret = 0;

	ctr_buf_reset(buf);

	uint64_t imsi;

	ret = ctr_lte_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
		return ret;
	}

	ret |= ctr_buf_append_u64(buf, imsi);
	ret |= ctr_buf_append_u16(buf, data->sample_count);

	if (!data->sample_count) {
		return ret ? -ENOSPC : 0;
	}

	ret |= ctr_buf_append_u64(buf, data->timestamp);

	LOG_INF("Populating %d sample(s)", data->sample_count);

	for (int i = 0; i < data->sample_count; i++) {
		ret |= ctr_buf_append_u16(buf, data->samples[i].timestamp_offset);
		ret |= ctr_buf_append_s32(buf, data->samples[i].avg_milliamps);
		ret |= ctr_buf_append_u32(buf, data->samples[i].rms_milliamps);
	}

	return ret ? -ENOSPC : 0;
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

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.report_interval * 1000 / 5);
	int64_t duration = g_app_config.report_interval * 1000 + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&m_send_timer, Z_TIMEOUT_MS(duration), K_FOREVER);

	CTR_BUF_DEFINE_STATIC(buf, 1024);

	ret = compose(&buf, &m_data);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 4100;

	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	m_data.sample_count = 0;

	ret = ctr_rtc_get_ts(&m_data.timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("New samples base timestamp: %llu", m_data.timestamp);

	return 0;
}

void measure_timer(struct k_timer *timer_id)
{
	LOG_INF("Measurement timer expired");

	atomic_set(&m_measure, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_measure_timer, measure_timer, NULL);

static int measure(void)
{
	int ret;

	k_timer_start(&m_measure_timer, Z_TIMEOUT_MS(g_app_config.scan_interval * 1000), K_FOREVER);

	if (m_data.sample_count >= ARRAY_SIZE(m_data.samples)) {
		LOG_WRN("Buffer sample full");
		return -ENOSPC;
	}

	int64_t timestamp;

	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	if (!g_app_config.sensor_test) {
		ret = ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 1);
		if (ret) {
			LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
			return ret;
		}
	}

	k_sleep(K_MSEC(1000));

	double avg;
	double rms;

	ret = ctr_signal_measure(&avg, &rms);
	if (ret) {
		LOG_ERR("Call `ctr_signal_measure` failed: %d", ret);
		return ret;
	}

	if (!g_app_config.sensor_test) {
		ret = ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 0);
		if (ret) {
			LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
			return ret;
		}
	}

	int current_avg = avg * (1000 * g_app_config.current_range / FULLSCALE_MV);
	int current_rms = rms * (1000 * g_app_config.current_range / FULLSCALE_MV);

	LOG_INF("Current AVG: %d mA RMS: %d mA", current_avg, current_rms);

	m_data.samples[m_data.sample_count].timestamp_offset = timestamp - m_data.timestamp;
	m_data.samples[m_data.sample_count].avg_milliamps = current_avg;
	m_data.samples[m_data.sample_count].rms_milliamps = current_rms;

	m_data.sample_count++;

	return 0;
}

static int loop(void)
{
	int ret;

	if (atomic_get(&m_measure)) {
		atomic_set(&m_measure, false);

		ret = measure();
		if (ret) {
			LOG_ERR("Call `measure` failed: %d", ret);
			return ret;
		}
	}

	if (atomic_get(&m_send)) {
		atomic_set(&m_send, false);

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
		LOG_ERR("Call `device_is_ready` failed");
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
		LOG_ERR("Call `device_is_ready` failed");
		k_oops();
	}

	wdt_feed(m_wdt_dev, m_wdt_id);
}

void main(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	init_wdt();

	ret = ctr_lte_set_event_cb(lte_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}

	ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_B3, CTR_GPIO_MODE_OUTPUT);
	if (ret) {
		LOG_ERR("Failed initializing GPIO channel B3: %d", ret);
		k_oops();
	}

	if (g_app_config.sensor_test) {
		ret = ctr_gpio_write(CTR_GPIO_CHANNEL_B3, 1);
		if (ret) {
			LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
			k_oops();
		}
	}

	for (;;) {
		feed_wdt();

		ret = k_sem_take(&m_run_sem, K_FOREVER);
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_oops();
		}

		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = ctr_rtc_get_ts(&m_data.timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		k_oops();
	}

	k_sleep(K_SECONDS(2));

	k_timer_start(&m_measure_timer, Z_TIMEOUT_MS(g_app_config.scan_interval * 1000), K_FOREVER);

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

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = measure();
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
