#include "app_config.h"

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

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/adc.h>
#include <logging/log.h>
#include <random/rand32.h>
#include <tinycbor/cbor.h>
#include <tinycbor/cbor_buf_writer.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zephyr.h>

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* TODO Would be nice to define using K_SECONDS, etc. Proper macros? */
#define BATT_TEST_INTERVAL_MSEC (6 * 60 * 60 * 1000)
#define MAX_REPETITIONS 5
#define MAX_DIFFERENCE 100

struct measurement {
	int timestamp_offset;
	int32_t a1_raw;
	int32_t a2_raw;
	int32_t b1_raw;
	int32_t b2_raw;
};

struct data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float accel_x;
	float accel_y;
	float accel_z;
	int accel_orientation;
	int measurement_count;
	int64_t measurement_timestamp;
	struct measurement measurements[128];
};

static atomic_t m_measure = true;
static atomic_t m_send = true;
static bool m_lte_eval_valid;
static K_MUTEX_DEFINE(m_lte_eval_mut);
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static K_SEM_DEFINE(m_run_sem, 0, 1);
static struct ctr_lte_eval m_lte_eval;
static struct data m_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.accel_x = NAN,
	.accel_y = NAN,
	.accel_z = NAN,
	.accel_orientation = INT_MAX,
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

static void measurement_timer(struct k_timer *timer_id)
{
	LOG_INF("Measurement timer expired");

	atomic_set(&m_measure, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_measurement_timer, measurement_timer, NULL);

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

static int measure_weight(const char *id, enum measure_weight_slot slot,
                          enum ctr_x3_channel channel, int32_t *result)
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

		ret = measure_weight(id, slot, channel, result);
		if (ret) {
			LOG_ERR("Call `measure_weight` failed: %d", ret);
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

static int measure(void)
{
	int ret;

	k_timer_start(&m_measurement_timer, Z_TIMEOUT_MS(g_app_config.measurement_interval * 1000),
	              K_FOREVER);

#if IS_ENABLED(CONFIG_CTR_THERM)
	ret = ctr_therm_read(&m_data.therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		m_data.therm_temperature = NAN;
	}
#endif

	ret = ctr_accel_read(&m_data.accel_x, &m_data.accel_y, &m_data.accel_z,
	                     &m_data.accel_orientation);
	if (ret) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		m_data.accel_x = NAN;
		m_data.accel_y = NAN;
		m_data.accel_z = NAN;
		m_data.accel_orientation = INT_MAX;
	}

	if (m_data.measurement_count >= ARRAY_SIZE(m_data.measurements)) {
		LOG_WRN("Measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x3_a), okay)
	static int32_t a1_raw_prev = INT32_MAX;
	ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw, &a1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
		a1_raw = INT32_MAX;
	}
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x3_a), okay)
	static int32_t a2_raw_prev = INT32_MAX;
	ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw, &a2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
		a2_raw = INT32_MAX;
	}
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x3_b), okay)
	static int32_t b1_raw_prev = INT32_MAX;
	ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw, &b1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
		b1_raw = INT32_MAX;
	}
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x3_b), okay)
	static int32_t b2_raw_prev = INT32_MAX;
	ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw, &b2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
		b2_raw = INT32_MAX;
	}
#endif

	m_data.measurements[m_data.measurement_count].timestamp_offset =
	        timestamp - m_data.measurement_timestamp;
	m_data.measurements[m_data.measurement_count].a1_raw = a1_raw;
	m_data.measurements[m_data.measurement_count].a2_raw = a2_raw;
	m_data.measurements[m_data.measurement_count].b1_raw = b1_raw;
	m_data.measurements[m_data.measurement_count].b2_raw = b2_raw;

	m_data.measurement_count++;

	return 0;
}

enum {
	MSG_KEY_FRAME = 0,
	MSG_KEY_SEQUENCE = 1,
	MSG_KEY_PROTOCOL = 2,
	MSG_KEY_ATTRIBUTES = 3,
	MSG_KEY_VENDOR_NAME = 4,
	MSG_KEY_PRODUCT_NAME = 5,
	MSG_KEY_HW_VARIANT = 6,
	MSG_KEY_HW_REVISION = 7,
	MSG_KEY_FW_VERSION = 8,
	MSG_KEY_SERIAL_NUMBER = 9,
	MSG_KEY_STATE = 10,
	MSG_KEY_UPTIME = 11,
	MSG_KEY_BATTERY = 12,
	MSG_KEY_VOLTAGE_REST = 13,
	MSG_KEY_VOLTAGE_LOAD = 14,
	MSG_KEY_CURRENT_LOAD = 15,
	MSG_KEY_NETWORK = 16,
	MSG_KEY_IMEI = 17,
	MSG_KEY_IMSI = 18,
	MSG_KEY_PARAMS = 19,
	MSG_KEY_EEST = 20,
	MSG_KEY_ECL = 21,
	MSG_KEY_RSRP = 22,
	MSG_KEY_RSRQ = 23,
	MSG_KEY_SNR = 24,
	MSG_KEY_PLMN = 25,
	MSG_KEY_CID = 26,
	MSG_KEY_BAND = 27,
	MSG_KEY_EARFCN = 28,
	MSG_KEY_THERMOMETER = 29,
	MSG_KEY_TEMPERATURE = 30,
	MSG_KEY_ACCELEROMETER = 31,
	MSG_KEY_ACCEL_X = 32,
	MSG_KEY_ACCEL_Y = 33,
	MSG_KEY_ACCEL_Z = 34,
	MSG_KEY_ORIENTATION = 35,
	MSG_KEY_MEASUREMENTS = 36,
	MSG_KEY_TIMESTAMP = 37,
	MSG_KEY_LOAD_CELL = 38,
	MSG_KEY_A1_RAW = 39,
	MSG_KEY_A2_RAW = 40,
	MSG_KEY_B1_RAW = 41,
	MSG_KEY_B2_RAW = 42,
};

static int compose(struct ctr_buf *buf)
{
	int ret;

	ctr_buf_reset(buf);
	ret = ctr_buf_seek(buf, 8);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	CborError err = 0;

	struct cbor_buf_writer writer;
	cbor_buf_writer_init(&writer, ctr_buf_get_mem(buf) + 8, ctr_buf_get_free(buf));

	CborEncoder enc;
	cbor_encoder_init(&enc, &writer.enc, 0);

	CborEncoder map;
	err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

	err |= cbor_encode_uint(&map, MSG_KEY_FRAME);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		static uint32_t sequence;
		err |= cbor_encode_uint(&map, MSG_KEY_SEQUENCE);
		err |= cbor_encode_uint(&map, sequence++);

		uint8_t protocol = 3;
		err |= cbor_encode_uint(&map, MSG_KEY_PROTOCOL);
		err |= cbor_encode_uint(&map, protocol);

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_ATTRIBUTES);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		char *vendor_name;
		ctr_info_get_vendor_name(&vendor_name);

		err |= cbor_encode_uint(&map, MSG_KEY_VENDOR_NAME);
		err |= cbor_encode_text_stringz(&map, vendor_name);

		char *product_name;
		ctr_info_get_product_name(&product_name);

		err |= cbor_encode_uint(&map, MSG_KEY_PRODUCT_NAME);
		err |= cbor_encode_text_stringz(&map, product_name);

		char *hw_variant;
		ctr_info_get_hw_variant(&hw_variant);

		err |= cbor_encode_uint(&map, MSG_KEY_HW_VARIANT);
		err |= cbor_encode_text_stringz(&map, hw_variant);

		char *hw_revision;
		ctr_info_get_hw_revision(&hw_revision);

		err |= cbor_encode_uint(&map, MSG_KEY_HW_REVISION);
		err |= cbor_encode_text_stringz(&map, hw_revision);

		char *fw_version;
		ctr_info_get_fw_version(&fw_version);

		err |= cbor_encode_uint(&map, MSG_KEY_FW_VERSION);
		err |= cbor_encode_text_stringz(&map, fw_version);

		char *serial_number;
		ctr_info_get_serial_number(&serial_number);

		err |= cbor_encode_uint(&map, MSG_KEY_SERIAL_NUMBER);
		err |= cbor_encode_text_stringz(&map, serial_number);

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_STATE);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_UPTIME);
		err |= cbor_encode_uint(&map, k_uptime_get() / 1000);

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_BATTERY);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE_REST);
		if (isnan(m_data.batt_voltage_rest)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, m_data.batt_voltage_rest * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_VOLTAGE_LOAD);
		if (isnan(m_data.batt_voltage_load)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, m_data.batt_voltage_load * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_CURRENT_LOAD);
		if (isnan(m_data.batt_current_load)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, m_data.batt_current_load);
		}

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_NETWORK);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		uint64_t imei;
		ret = ctr_lte_get_imei(&imei);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
			return ret;
		}

		err |= cbor_encode_uint(&map, MSG_KEY_IMEI);
		err |= cbor_encode_uint(&map, imei);

		uint64_t imsi;
		ret = ctr_lte_get_imsi(&imsi);
		if (ret) {
			LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
			return ret;
		}

		err |= cbor_encode_uint(&map, MSG_KEY_IMSI);
		err |= cbor_encode_uint(&map, imsi);

		err |= cbor_encode_uint(&map, MSG_KEY_PARAMS);
		{
			CborEncoder map;
			err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

			k_mutex_lock(&m_lte_eval_mut, K_FOREVER);

			err |= cbor_encode_uint(&map, MSG_KEY_EEST);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.eest);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_ECL);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.ecl);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_RSRP);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.rsrp);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_RSRQ);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.rsrq);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_SNR);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.snr);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_PLMN);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.plmn);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_CID);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.cid);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_BAND);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.band);
			} else {
				err |= cbor_encode_null(&map);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_EARFCN);
			if (m_lte_eval_valid) {
				err |= cbor_encode_int(&map, m_lte_eval.earfcn);
			} else {
				err |= cbor_encode_null(&map);
			}

			m_lte_eval_valid = false;

			k_mutex_unlock(&m_lte_eval_mut);

			err |= cbor_encoder_close_container(&enc, &map);
		}

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_THERMOMETER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_TEMPERATURE);
		if (isnan(m_data.therm_temperature)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, m_data.therm_temperature * 10.f);
		}

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_ACCELEROMETER);
	{
		CborEncoder map;
		err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_X);
		if (isnan(m_data.accel_x)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, m_data.accel_x * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_Y);
		if (isnan(m_data.accel_y)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, m_data.accel_y * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ACCEL_Z);
		if (isnan(m_data.accel_z)) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_int(&map, m_data.accel_z * 1000.f);
		}

		err |= cbor_encode_uint(&map, MSG_KEY_ORIENTATION);
		if (m_data.accel_orientation == INT_MAX) {
			err |= cbor_encode_null(&map);
		} else {
			err |= cbor_encode_uint(&map, m_data.accel_orientation);
		}

		err |= cbor_encoder_close_container(&enc, &map);
	}

	err |= cbor_encode_uint(&map, MSG_KEY_MEASUREMENTS);
	{
		CborEncoder arr;
		err |= cbor_encoder_create_array(&enc, &arr, CborIndefiniteLength);

		for (int i = 0; i < m_data.measurement_count; i++) {
			CborEncoder map;
			err |= cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);

			uint64_t timestamp = m_data.measurement_timestamp +
			                     m_data.measurements[i].timestamp_offset;

			err |= cbor_encode_uint(&map, MSG_KEY_TIMESTAMP);
			err |= cbor_encode_uint(&map, timestamp);

			err |= cbor_encode_uint(&map, MSG_KEY_A1_RAW);
			if (m_data.measurements[i].a1_raw == INT32_MAX) {
				err |= cbor_encode_null(&map);
			} else {
				err |= cbor_encode_int(&map, m_data.measurements[i].a1_raw);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_A2_RAW);
			if (m_data.measurements[i].a2_raw == INT32_MAX) {
				err |= cbor_encode_null(&map);
			} else {
				err |= cbor_encode_int(&map, m_data.measurements[i].a2_raw);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_B1_RAW);
			if (m_data.measurements[i].b1_raw == INT32_MAX) {
				err |= cbor_encode_null(&map);
			} else {
				err |= cbor_encode_int(&map, m_data.measurements[i].b1_raw);
			}

			err |= cbor_encode_uint(&map, MSG_KEY_B2_RAW);
			if (m_data.measurements[i].b2_raw == INT32_MAX) {
				err |= cbor_encode_null(&map);
			} else {
				err |= cbor_encode_int(&map, m_data.measurements[i].b2_raw);
			}

			err |= cbor_encoder_close_container(&enc, &map);
		}

		err |= cbor_encoder_close_container(&enc, &arr);
	}

	err |= cbor_encoder_close_container(&enc, &map);

	ret = ctr_buf_seek(buf, 8 + enc.writer->bytes_written);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	size_t len = ctr_buf_get_used(buf);

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

	return err ? -ENOSPC : 0;
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

	k_timer_start(&m_send_timer, Z_TIMEOUT_MS(duration), K_FOREVER);

	CTR_BUF_DEFINE_STATIC(buf, 512);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		if (ret == -ENOSPC) {
			m_data.measurement_count = 0;
		}
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 4103;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	m_data.measurement_count = 0;

	ret = ctr_rtc_get_ts(&m_data.measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("New samples base timestamp: %llu", m_data.measurement_timestamp);

	return 0;
}

static int loop(void)
{
	int ret;

	ret = task_battery();
	if (ret < 0) {
		LOG_ERR("Call `task_battery` failed: %d", ret);
		return ret;
	}

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

void main(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

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

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = ctr_rtc_get_ts(&m_data.measurement_timestamp);
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

	k_timer_start(&m_measurement_timer, K_NO_WAIT, K_FOREVER);

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

SHELL_CMD_REGISTER(m, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
