#include "app_config.h"
#include "msg_key.h"

/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_buf.h>
#include <ctr_info.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_rtc.h>
#include <ctr_therm.h>
#include <ctr_wdog.h>
#include <drivers/ctr_batt.h>
#include <drivers/ctr_x3.h>
#include <drivers/people_counter.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/adc.h>
#include <logging/log.h>
#include <random/rand32.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_encode.h>
#include <zephyr.h>

/* Standard includes */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* TODO Would be nice to define using K_SECONDS, etc. Proper macros? */
#define BATT_TEST_INTERVAL_MSEC (12 * 60 * 60 * 1000)
#define MAX_REPETITIONS 5
#define MAX_DIFFERENCE 100

struct weight_measurement {
	int timestamp_offset;
	int32_t a1_raw;
	int32_t a2_raw;
	int32_t b1_raw;
	int32_t b2_raw;
};

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
struct people_measurement {
	int timestamp_offset;
	struct people_counter_measurement measurement;
	bool is_valid;
};
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

struct data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float acceleration_x;
	float acceleration_y;
	float acceleration_z;
	int orientation;
	int weight_measurement_count;
	int64_t weight_measurement_timestamp;
	struct weight_measurement weight_measurements[128];
#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	int people_measurement_count;
	int64_t people_measurement_timestamp;
	struct people_measurement people_measurements[128];
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */
};

static atomic_t m_measure_weight = true;

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
static atomic_t m_measure_people = true;
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

static atomic_t m_send = true;
static bool m_lte_eval_valid;
static K_MUTEX_DEFINE(m_weight_reading_lock);
static K_MUTEX_DEFINE(m_lte_eval_mut);
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static K_SEM_DEFINE(m_run_sem, 0, 1);
static struct ctr_lte_eval m_lte_eval;
static struct data m_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.acceleration_x = NAN,
	.acceleration_y = NAN,
	.acceleration_z = NAN,
	.orientation = INT_MAX,
};
static struct ctr_wdog_channel m_wdog_channel;

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

static int task_battery(void)
{
	int ret;
	static int64_t next;

	if (k_uptime_get() < next) {
		return 0;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -EINVAL;
		goto error;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv, CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
		goto error;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(dev, &load_mv, CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
		goto error;
	}

	int current_ma;
	ctr_batt_get_load_current_ma(dev, &current_ma, load_mv);

	LOG_INF("Battery voltage (rest): %u mV", rest_mv);
	LOG_INF("Battery voltage (load): %u mV", load_mv);
	LOG_INF("Battery current (load): %u mA", current_ma);

	m_data.batt_voltage_rest = rest_mv / 1000.f;
	m_data.batt_voltage_load = load_mv / 1000.f;
	m_data.batt_current_load = current_ma;

	next = k_uptime_get() + BATT_TEST_INTERVAL_MSEC;

	return 0;

error:
	m_data.batt_voltage_rest = NAN;
	m_data.batt_voltage_load = NAN;
	m_data.batt_current_load = NAN;

	return ret;
}

static void weight_measurement_timer(struct k_timer *timer_id)
{
	LOG_INF("Weight measurement timer expired");

	atomic_set(&m_measure_weight, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_weight_measurement_timer, weight_measurement_timer, NULL);

static int compare(const void *a, const void *b)
{
	int32_t sample_a = *((int32_t *)a);
	int32_t sample_b = *((int32_t *)b);

	if (sample_a == sample_b) {
		return 0;
	} else if (sample_a < sample_b) {
		return -1;
	}

	return 1;
}

enum measure_weight_slot {
	MEASURE_WEIGHT_SLOT_A,
	MEASURE_WEIGHT_SLOT_B,
};

static int read_weight(const char *id, enum measure_weight_slot slot, enum ctr_x3_channel channel,
                       int32_t *result)
{
	int ret;

	static const struct device *ctr_x3_a_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_a));
	static const struct device *ctr_x3_b_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_b));

	const struct device *dev;

	switch (slot) {
	case MEASURE_WEIGHT_SLOT_A:
		dev = ctr_x3_a_dev;
		break;
	case MEASURE_WEIGHT_SLOT_B:
		dev = ctr_x3_b_dev;
		break;
	default:
		LOG_ERR("Unknown slot: %d", slot);
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	ret = ctr_x3_set_power(dev, channel, true);
	if (ret) {
		LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
		goto error;
	}

	k_sleep(K_MSEC(100));

	int32_t samples[32];
	int64_t average = 0;
	int64_t filtered = 0;

	for (size_t i = 0; i < ARRAY_SIZE(samples); i++) {
		int32_t sample;
		ret = ctr_x3_measure(dev, channel, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_x3_measure` failed: %d", ret);
			goto error;
		}

		samples[i] = sample;
		average += sample;
	}

	average /= ARRAY_SIZE(samples);

	qsort(samples, ARRAY_SIZE(samples), sizeof(samples[0]), compare);

	size_t start = ARRAY_SIZE(samples) / 4;
	size_t stop = 3 * ARRAY_SIZE(samples) / 4;

	for (size_t i = start; i < stop; i++) {
		filtered += samples[i];
	}

	filtered /= stop - start;

	ret = ctr_x3_set_power(dev, channel, false);
	if (ret) {
		LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
		goto error;
	}

	*result = filtered;

	LOG_DBG("Channel %s: Minimum: %" PRId32, id, samples[0]);
	LOG_DBG("Channel %s: Maximum: %" PRId32, id, samples[ARRAY_SIZE(samples) - 1]);
	LOG_DBG("Channel %s: Average: %" PRId32, id, (int32_t)average);
	LOG_DBG("Channel %s: Median: %" PRId32, id, samples[ARRAY_SIZE(samples) / 2]);
	LOG_DBG("Channel %s: Filtered: %" PRId32, id, (int32_t)filtered);

	return 0;

error:
	ctr_x3_set_power(dev, CTR_X3_CHANNEL_1, false);
	return ret;
}

static int filter_weight(const char *id, enum measure_weight_slot slot, enum ctr_x3_channel channel,
                         int32_t *result, int32_t *prev)
{
	int ret;

	int i;
	for (i = 0; i < MAX_REPETITIONS; i++) {
		if (i != 0) {
			LOG_INF("Channel %s: Repeating measurement (%d)", id, i);
		}

		ret = read_weight(id, slot, channel, result);
		if (ret) {
			LOG_ERR("Call `read_weight` failed: %d", ret);
			return ret;
		}

		if (*prev == INT32_MAX) {
			break;
		}

		int32_t diff = MAX(*result, *prev) - MIN(*result, *prev);

		if (diff < MAX_DIFFERENCE) {
			break;
		} else {
			*prev = *result;
		}
	}

	*prev = *result;

	if (i == MAX_REPETITIONS) {
		*result ^= BIT(30);
		LOG_WRN("Channel %s: Inverted ERROR flag (bit 30)", id);
	}

	return 0;
}

static int measure_weight(void)
{
	int ret;

	k_timer_start(&m_weight_measurement_timer,
	              Z_TIMEOUT_MS(g_app_config.weight_measurement_interval * 1000), K_FOREVER);

	if (m_data.weight_measurement_count >= ARRAY_SIZE(m_data.weight_measurements)) {
		LOG_WRN("Weight measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	k_mutex_lock(&m_weight_reading_lock, K_FOREVER);

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if defined(CONFIG_SHIELD_CTR_X3_A)
	if (g_app_config.channel_a1_active) {
		static int32_t a1_raw_prev = INT32_MAX;
		ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw,
		                    &a1_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
			a1_raw = INT32_MAX;
		}
	}
#endif

#if defined(CONFIG_SHIELD_CTR_X3_A)
	if (g_app_config.channel_a2_active) {
		static int32_t a2_raw_prev = INT32_MAX;
		ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw,
		                    &a2_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
			a2_raw = INT32_MAX;
		}
	}
#endif

#if defined(CONFIG_SHIELD_CTR_X3_B)
	if (g_app_config.channel_b1_active) {
		static int32_t b1_raw_prev = INT32_MAX;
		ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw,
		                    &b1_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
			b1_raw = INT32_MAX;
		}
	}
#endif

#if defined(CONFIG_SHIELD_CTR_X3_B)
	if (g_app_config.channel_b2_active) {
		static int32_t b2_raw_prev = INT32_MAX;
		ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw,
		                    &b2_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
			b2_raw = INT32_MAX;
		}
	}
#endif

	k_mutex_unlock(&m_weight_reading_lock);

	m_data.weight_measurements[m_data.weight_measurement_count].timestamp_offset =
	        timestamp - m_data.weight_measurement_timestamp;

	m_data.weight_measurements[m_data.weight_measurement_count].a1_raw = a1_raw;
	m_data.weight_measurements[m_data.weight_measurement_count].a2_raw = a2_raw;
	m_data.weight_measurements[m_data.weight_measurement_count].b1_raw = b1_raw;
	m_data.weight_measurements[m_data.weight_measurement_count].b2_raw = b2_raw;

	m_data.weight_measurement_count++;

	return 0;
}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)

static void people_measurement_timer(struct k_timer *timer_id)
{
	LOG_INF("People measurement timer expired");

	atomic_set(&m_measure_people, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_people_measurement_timer, people_measurement_timer, NULL);

static int measure_people(void)
{
	int ret;

	k_timer_start(&m_people_measurement_timer,
	              Z_TIMEOUT_MS(g_app_config.people_measurement_interval * 1000), K_FOREVER);

	if (m_data.people_measurement_count >= ARRAY_SIZE(m_data.people_measurements)) {
		LOG_WRN("People measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(people_counter));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	m_data.people_measurements[m_data.people_measurement_count].timestamp_offset =
	        timestamp - m_data.people_measurement_timestamp;

	struct people_counter_measurement measurement;
	ret = people_counter_read_measurement(dev, &measurement);
	if (ret) {
		LOG_ERR("Call `people_counter_read_measurement` failed: %d", ret);

		m_data.people_measurements[m_data.people_measurement_count].is_valid = false;

	} else {
		LOG_INF("Motion counter: %u", measurement.motion_counter);
		LOG_INF("Pass counter (adult): %u", measurement.pass_counter_adult);
		LOG_INF("Pass counter (child): %u", measurement.pass_counter_child);
		LOG_INF("Stay counter (adult): %u", measurement.stay_counter_adult);
		LOG_INF("Stay counter (child): %u", measurement.stay_counter_child);
		LOG_INF("Total time (adult): %u", measurement.total_time_adult);
		LOG_INF("Total time (child): %u", measurement.total_time_child);
		LOG_INF("Consumed energy: %u", measurement.consumed_energy);

		m_data.people_measurements[m_data.people_measurement_count].measurement =
		        measurement;

		m_data.people_measurements[m_data.people_measurement_count].is_valid = true;
	}

	m_data.people_measurement_count++;

	return 0;
}

#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

static int measure(void)
{
	int ret;

#if defined(CONFIG_CTR_THERM)
	ret = ctr_therm_read(&m_data.therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		m_data.therm_temperature = NAN;
	}
#endif

	ret = ctr_accel_read(&m_data.acceleration_x, &m_data.acceleration_y, &m_data.acceleration_z,
	                     &m_data.orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		m_data.acceleration_x = NAN;
		m_data.acceleration_y = NAN;
		m_data.acceleration_z = NAN;
		m_data.orientation = INT_MAX;
	}

	return 0;
}

static int encode(zcbor_state_t *zs)
{
	int ret;

	zs->constant_state->stop_on_error = true;

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, MSG_KEY_FRAME);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		static uint32_t sequence;
		zcbor_uint32_put(zs, MSG_KEY_SEQUENCE);
		zcbor_uint32_put(zs, sequence++);

		uint8_t protocol = 4;
		zcbor_uint32_put(zs, MSG_KEY_PROTOCOL);
		zcbor_uint32_put(zs, protocol);

		uint64_t timestamp;
		ret = ctr_rtc_get_ts(&timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_TIMESTAMP);
		zcbor_uint64_put(zs, timestamp);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_ATTRIBUTE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		zcbor_uint32_put(zs, MSG_KEY_VENDOR_NAME);
		zcbor_tstr_put_term(zs, vendor_name);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		zcbor_uint32_put(zs, MSG_KEY_PRODUCT_NAME);
		zcbor_tstr_put_term(zs, product_name);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		zcbor_uint32_put(zs, MSG_KEY_HW_VARIANT);
		zcbor_tstr_put_term(zs, hw_variant);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		zcbor_uint32_put(zs, MSG_KEY_HW_REVISION);
		zcbor_tstr_put_term(zs, hw_revision);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		zcbor_uint32_put(zs, MSG_KEY_FW_NAME);
		zcbor_tstr_put_term(zs, "CHESTER Scale");

		zcbor_uint32_put(zs, MSG_KEY_FW_VERSION);
		zcbor_tstr_put_term(zs, fw_version);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		zcbor_uint32_put(zs, MSG_KEY_SERIAL_NUMBER);
		zcbor_tstr_put_term(zs, serial_number);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_STATE);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_UPTIME);
		zcbor_uint64_put(zs, k_uptime_get() / 1000);

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_BATTERY);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_REST);
		if (isnan(m_data.batt_voltage_rest)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, m_data.batt_voltage_rest * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_VOLTAGE_LOAD);
		if (isnan(m_data.batt_voltage_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, m_data.batt_voltage_load * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_CURRENT_LOAD);
		if (isnan(m_data.batt_current_load)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, m_data.batt_current_load);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_NETWORK);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		uint64_t imei;
		ret = ctr_lte_get_imei(&imei);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_IMEI);
		zcbor_uint64_put(zs, imei);

		uint64_t imsi;
		ret = ctr_lte_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
			return ret;
		}

		zcbor_uint32_put(zs, MSG_KEY_IMSI);
		zcbor_uint64_put(zs, imsi);

		zcbor_uint32_put(zs, MSG_KEY_PARAMETER);
		{
			zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			k_mutex_lock(&m_lte_eval_mut, K_FOREVER);

			zcbor_uint32_put(zs, MSG_KEY_EEST);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.eest);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_ECL);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.ecl);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_RSRP);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.rsrp);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_RSRQ);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.rsrq);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_SNR);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.snr);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_PLMN);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.plmn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_CID);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.cid);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_BAND);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.band);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			zcbor_uint32_put(zs, MSG_KEY_EARFCN);
			if (m_lte_eval_valid) {
				zcbor_int32_put(zs, m_lte_eval.earfcn);
			} else {
				zcbor_nil_put(zs, NULL);
			}

			m_lte_eval_valid = false;

			k_mutex_unlock(&m_lte_eval_mut);

			zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_THERMOMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_TEMPERATURE);
		if (isnan(m_data.therm_temperature)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, m_data.therm_temperature * 100.f);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	zcbor_uint32_put(zs, MSG_KEY_ACCELEROMETER);
	{
		zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_X);
		if (isnan(m_data.acceleration_x)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, m_data.acceleration_x * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Y);
		if (isnan(m_data.acceleration_y)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, m_data.acceleration_y * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ACCELERATION_Z);
		if (isnan(m_data.acceleration_z)) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_int32_put(zs, m_data.acceleration_z * 1000.f);
		}

		zcbor_uint32_put(zs, MSG_KEY_ORIENTATION);
		if (m_data.orientation == INT_MAX) {
			zcbor_nil_put(zs, NULL);
		} else {
			zcbor_uint32_put(zs, m_data.orientation);
		}

		zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}

	if (g_app_config.channel_a1_active || g_app_config.channel_a2_active ||
	    g_app_config.channel_b1_active || g_app_config.channel_b2_active) {
		zcbor_uint32_put(zs, MSG_KEY_WEIGHT_MEASUREMENTS);
		{
			zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

			zcbor_uint64_put(zs, m_data.weight_measurement_timestamp);

			for (int i = 0; i < m_data.weight_measurement_count; i++) {
				zcbor_uint32_put(zs,
				                 m_data.weight_measurements[i].timestamp_offset);

				if (g_app_config.channel_a1_active) {
					if (m_data.weight_measurements[i].a1_raw == INT32_MAX) {
						zcbor_nil_put(zs, NULL);
					} else {
						zcbor_int32_put(
						        zs, m_data.weight_measurements[i].a1_raw);
					}
				}

				if (g_app_config.channel_a2_active) {
					if (m_data.weight_measurements[i].a2_raw == INT32_MAX) {
						zcbor_nil_put(zs, NULL);
					} else {
						zcbor_int32_put(
						        zs, m_data.weight_measurements[i].a2_raw);
					}
				}

				if (g_app_config.channel_b1_active) {
					if (m_data.weight_measurements[i].b1_raw == INT32_MAX) {
						zcbor_nil_put(zs, NULL);
					} else {
						zcbor_int32_put(
						        zs, m_data.weight_measurements[i].b1_raw);
					}
				}

				if (g_app_config.channel_b2_active) {
					if (m_data.weight_measurements[i].b2_raw == INT32_MAX) {
						zcbor_nil_put(zs, NULL);
					} else {
						zcbor_int32_put(
						        zs, m_data.weight_measurements[i].b2_raw);
					}
				}
			}

			zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
		}
	}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	zcbor_uint32_put(zs, MSG_KEY_PEOPLE_MEASUREMENTS);
	{
		zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

		zcbor_uint64_put(zs, m_data.people_measurement_timestamp);

		for (int i = 0; i < m_data.people_measurement_count; i++) {
			bool is_valid = m_data.people_measurements[i].is_valid;

			struct people_counter_measurement *measurement =
			        &m_data.people_measurements[i].measurement;

			zcbor_uint32_put(zs, m_data.people_measurements[i].timestamp_offset);

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->motion_counter);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->pass_counter_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->pass_counter_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->stay_counter_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->stay_counter_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->total_time_adult);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->total_time_child);
			}

			if (!is_valid) {
				zcbor_nil_put(zs, NULL);
			} else {
				zcbor_uint32_put(zs, measurement->consumed_energy);
			}
		}

		zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	if (!zcbor_check_error(zs)) {
		LOG_ERR("Encoding failed: %d", zcbor_pop_error(zs));
		return -EFAULT;
	}

	return 0;
}

static int compose(struct ctr_buf *buf)
{
	int ret;

	ctr_buf_reset(buf);
	ret = ctr_buf_seek(buf, 8);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u32(buf, serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u32` failed: %d", ret);
		return ret;
	}

	zcbor_state_t zs[8];
	zcbor_new_state(zs, ARRAY_SIZE(zs), ctr_buf_get_mem(buf) + 12, ctr_buf_get_free(buf), 0);

	ret = encode(zs);
	if (ret) {
		LOG_ERR("Call `encode` failed: %d", ret);
		return ret;
	}

	size_t len = zs[0].payload_mut - ctr_buf_get_mem(buf);

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return ret;
	}

	ret = tc_sha256_update(&s, ctr_buf_get_mem(buf) + 8, len - 8);
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

	ret = ctr_buf_seek(buf, 0);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u16(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, digest, 6);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void send_timer(struct k_timer *timer_id)
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

	k_timer_start(&m_send_timer, K_MSEC(duration), K_FOREVER);

	ret = measure();
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		return ret;
	}

	CTR_BUF_DEFINE_STATIC(buf, 1024);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		if (ret == -ENOSPC) {
			m_data.weight_measurement_count = 0;

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
			m_data.people_measurement_count = 0;
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */
		}
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 5000;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	m_data.weight_measurement_count = 0;

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	m_data.people_measurement_count = 0;
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	ret = ctr_rtc_get_ts(&m_data.weight_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("New samples base timestamp: %llu", m_data.weight_measurement_timestamp);

	return 0;
}

static int loop(void)
{
	int ret;

	ret = task_battery();
	if (ret) {
		LOG_ERR("Call `task_battery` failed: %d", ret);
		return ret;
	}

	if (atomic_set(&m_measure_weight, false)) {
		ret = measure_weight();
		if (ret) {
			LOG_ERR("Call `measure_weight` failed: %d", ret);
			return ret;
		}
	}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	if (atomic_set(&m_measure_people, false)) {
		ret = measure_people();
		if (ret) {
			LOG_ERR("Call `measure_people` failed: %d", ret);
			return ret;
		}
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	if (atomic_set(&m_send, false)) {
		ret = send();
		if (ret) {
			LOG_ERR("Call `send` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
static int init_people_counter(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(people_counter));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = people_counter_set_power_off_delay(dev, g_app_config.people_counter_power_off_delay);
	if (ret) {
		LOG_ERR("Call `people_counter_set_power_off_delay` failed: %d", ret);
		return ret;
	}

	ret = people_counter_set_stay_timeout(dev, g_app_config.people_counter_stay_timeout);
	if (ret) {
		LOG_ERR("Call `people_counter_set_stay_timeout` failed: %d", ret);
		return ret;
	}

	ret = people_counter_set_adult_border(dev, g_app_config.people_counter_adult_border);
	if (ret) {
		LOG_ERR("Call `people_counter_set_adult_border` failed: %d", ret);
		return ret;
	}

	return 0;
}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ctr_led_set(CTR_LED_CHANNEL_R, true);
	k_sleep(K_SECONDS(2));
	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_install(&m_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		k_oops();
	}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	ret = init_people_counter();
	if (ret) {
		LOG_ERR("Call `init_people_counter` failed: %d", ret);
		k_oops();
	}
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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
		ret = ctr_wdog_feed(&m_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

		ret = k_sem_take(&m_run_sem, K_FOREVER);
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_oops();
		}

		break;
	}

	ret = ctr_rtc_get_ts(&m_data.weight_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_wdog_feed(&m_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

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
		}
	}
}

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&m_weight_measurement_timer, K_NO_WAIT, K_FOREVER);

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	k_timer_start(&m_people_measurement_timer, K_NO_WAIT, K_FOREVER);
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&m_send_timer, K_NO_WAIT, K_FOREVER);

	return 0;
}

static int cmd_test(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_mutex_lock(&m_weight_reading_lock, K_FOREVER);

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if defined(CONFIG_SHIELD_CTR_X3_A)
	static int32_t a1_raw_prev = INT32_MAX;
	ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw, &a1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
		shell_error(shell, "channel measurement failed (a1)");
		a1_raw = INT32_MAX;
	}
#endif

	if (a1_raw == INT32_MAX) {
		shell_print(shell, "raw (a1): (null)");
	} else {
		shell_print(shell, "raw (a1): %" PRId32, a1_raw);
	}

#if defined(CONFIG_SHIELD_CTR_X3_A)
	static int32_t a2_raw_prev = INT32_MAX;
	ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw, &a2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
		shell_error(shell, "channel measurement failed (a2)");
		a2_raw = INT32_MAX;
	}
#endif

	if (a2_raw == INT32_MAX) {
		shell_print(shell, "raw (a2): (null)");
	} else {
		shell_print(shell, "raw (a2): %" PRId32, a2_raw);
	}

#if defined(CONFIG_SHIELD_CTR_X3_B)
	static int32_t b1_raw_prev = INT32_MAX;
	ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw, &b1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
		shell_error(shell, "channel measurement failed (b1)");
		b1_raw = INT32_MAX;
	}
#endif

	if (b1_raw == INT32_MAX) {
		shell_print(shell, "raw (b1): (null)");
	} else {
		shell_print(shell, "raw (b1): %" PRId32, b1_raw);
	}

#if defined(CONFIG_SHIELD_CTR_X3_B)
	static int32_t b2_raw_prev = INT32_MAX;
	ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw, &b2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
		shell_error(shell, "channel measurement failed (b2)");
		b2_raw = INT32_MAX;
	}
#endif

	if (b2_raw == INT32_MAX) {
		shell_print(shell, "raw (b2): (null)");
	} else {
		shell_print(shell, "raw (b2): %" PRId32, b2_raw);
	}

	k_mutex_unlock(&m_weight_reading_lock);

	return 0;
}

SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
SHELL_CMD_REGISTER(test, NULL, "Perform test measurement.", cmd_test);
