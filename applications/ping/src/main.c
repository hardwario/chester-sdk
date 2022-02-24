/* CHESTER includes */
#include <ctr_buf.h>
#include <ctr_led.h>
#include <ctr_lte.h>

/* Zephyr includes */
#include <logging/log.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static atomic_t m_send = true;
static bool m_lte_eval_valid;
static K_MUTEX_DEFINE(m_lte_eval_mut);
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static K_SEM_DEFINE(m_run_sem, 0, 1);
static struct ctr_lte_eval m_lte_eval;

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

	case CTR_LTE_EVENT_EVAL_OK:
		LOG_INF("Event `CTR_LTE_EVENT_EVAL_OK`");

		struct ctr_lte_eval *eval = &data->eval_ok.eval;

		LOG_DBG("EEST: %d", eval->eest);
		LOG_DBG("ECL: %d", eval->ecl);
		LOG_DBG("RSRP: %d", eval->rsrp);
		LOG_DBG("RSRQ: %d", eval->rsrq);
		LOG_DBG("SNR: %d", eval->snr);
		LOG_DBG("PLMN: %d", eval->plmn);
		LOG_DBG("CID: %d", eval->cid);
		LOG_DBG("BAND: %d", eval->band);
		LOG_DBG("EARFCN: %d", eval->earfcn);

		k_mutex_lock(&m_lte_eval_mut, K_FOREVER);
		memcpy(&m_lte_eval, &data->eval_ok.eval, sizeof(m_lte_eval));
		m_lte_eval_valid = true;
		k_mutex_unlock(&m_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_EVAL_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_EVAL_ERR`");

		k_mutex_lock(&m_lte_eval_mut, K_FOREVER);
		m_lte_eval_valid = false;
		k_mutex_unlock(&m_lte_eval_mut);

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

	k_timer_start(&m_send_timer, K_SECONDS(60), K_FOREVER);

	CTR_BUF_DEFINE(buf, 128);
	ctr_buf_seek(&buf, 2);

	uint64_t imsi;
	ret = ctr_lte_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
		return -EBUSY;
	}

	ctr_buf_append_u64(&buf, imsi);

	static uint32_t counter;
	ctr_buf_append_u32(&buf, counter++);

	k_mutex_lock(&m_lte_eval_mut, K_FOREVER);

	if (m_lte_eval_valid) {
		ctr_buf_append_s32(&buf, m_lte_eval.eest);
		ctr_buf_append_s32(&buf, m_lte_eval.ecl);
		ctr_buf_append_s32(&buf, m_lte_eval.rsrp);
		ctr_buf_append_s32(&buf, m_lte_eval.rsrq);
		ctr_buf_append_s32(&buf, m_lte_eval.snr);
		ctr_buf_append_s32(&buf, m_lte_eval.plmn);
		ctr_buf_append_s32(&buf, m_lte_eval.cid);
		ctr_buf_append_s32(&buf, m_lte_eval.band);
		ctr_buf_append_s32(&buf, m_lte_eval.earfcn);
	}

	m_lte_eval_valid = false;

	k_mutex_unlock(&m_lte_eval_mut);

	size_t len = ctr_buf_get_used(&buf);
	ctr_buf_seek(&buf, 0);
	ctr_buf_append_s16(&buf, len + 6);
	ctr_buf_seek(&buf, len);

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return ret;
	}

	ret = tc_sha256_update(&s, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_update` failed: %d", ret);
		return ret;
	}

	uint8_t digest[32];
	ret = tc_sha256_final(digest, &s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_final` failed: %d", ret);
		return ret;
	}

	ctr_buf_append_mem(&buf, digest, 6);

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

#if 1
	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
#else
	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS_PUBLIC_IP;
#endif
	opts.port = 4101;

	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void init_wdt(void)
{
	/*
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
	*/
}

static void feed_wdt(void)
{
	/*
	if (!device_is_ready(m_wdt_dev)) {
		LOG_ERR("Call `device_is_ready` failed");
		k_oops();
	}

	wdt_feed(m_wdt_dev, m_wdt_id);
	*/
}

static int loop(void)
{
	int ret;

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
