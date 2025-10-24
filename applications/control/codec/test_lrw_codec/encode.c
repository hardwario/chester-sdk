/** @file
 *  @brief cloud packet test suite
 *
 */

#include <app_lrw.h>
#include <app_data.h>

#include <chester/ctr_test_lrw.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/base64.h>

#include <zephyr/logging/log.h>
#include <cJSON.h>

#include <stdlib.h>

#include <float.h>
#include <math.h>
LOG_MODULE_REGISTER(lrw_codec, LOG_LEVEL_DBG);

void write_base64_file(const char *filename, struct ctr_buf *lrw_buf)
{
	FILE *f_base64 = fopen(filename, "w");
	if (f_base64) {
		const uint8_t *data = ctr_buf_get_mem(lrw_buf);
		size_t len = ctr_buf_get_used(lrw_buf);
		uint8_t b64_encoded[512];
		size_t olen;
		base64_encode(b64_encoded, sizeof(b64_encoded), &olen, data, len);
		fputs((char *)b64_encoded, f_base64);
		fclose(f_base64);
	} else {
		LOG_ERR("Failed to open base64 file for writing");
	}
}

void write_hex_file(char *filename, struct ctr_buf *buf)
{
	FILE *f = fopen(filename, "w");
	if (f) {
		const uint8_t *data = ctr_buf_get_mem(buf);
		size_t len = ctr_buf_get_used(buf);
		for (size_t i = 0; i < len; ++i) {
			fprintf(f, "%02X", data[i]);
		}
		fprintf(f, "\n");
		fclose(f);
	} else {
		LOG_ERR("Failed to open file for writing");
	}
}

void create_output_files(const char *filename, cJSON *root)
{
	char *json_str = cJSON_Print(root);
	char filename_json[255];
	snprintf(filename_json, sizeof(filename_json), "data/%s.json", filename);
	FILE *f = fopen(filename_json, "w");
	zassert_not_null(f, "Failed to open file for writing");
	fputs(json_str, f);
	fclose(f);
	cJSON_free(json_str);

	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
	app_lrw_encode(&lrw_buf);
	char filename_b64[255];
	snprintf(filename_b64, sizeof(filename_b64), "data/%s.b64", filename);
	write_base64_file(filename_b64, &lrw_buf);
}

ZTEST(app_control_lrw, test_positive)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "data", data);

#if defined(CONFIG_CTR_BATT)
	g_app_data.system_voltage_rest = 3250.f;
	g_app_data.system_voltage_load = 3125.f;
	g_app_data.system_current_load = 15;
	CTR_TEST_LRW_JSON_BATT(data);
#endif

#if defined(CONFIG_CTR_ACCEL)
	g_app_data.accel_orientation = 3;
	CTR_TEST_LRW_JSON_ORIENTATION(data);
#endif

#if defined(CONFIG_CTR_THERM)
	g_app_data.therm_temperature = 23.25f;
	CTR_TEST_LRW_JSON_THERM(data);
#endif

// Hygro
#if defined(FEATURE_HARDWARE_CHESTER_S2)
	g_app_data.hygro.last_sample_temperature = 21.25f;
	g_app_data.hygro.last_sample_humidity = 45;
	CTR_TEST_LRW_JSON_HYGRO(data);
#endif

// 1-Wire
#if defined(FEATURE_SUBSYSTEM_DS18B20)
	g_app_data.w1_therm.sensor[0].last_sample_temperature = 18.25f;
	g_app_data.w1_therm.sensor[0].serial_number = 0x123456789ABCDEF0;
	g_app_data.w1_therm.sensor[1].last_sample_temperature = 20.25f;
	g_app_data.w1_therm.sensor[1].serial_number = 0x123456789ABCDEF1;
	CTR_TEST_LRW_JSON_W1_THERMOMETERS(data);
#endif

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	// BLE Tags
	g_app_data.ble_tag.sensor[0].last_sample_temperature = 5.25f;
	g_app_data.ble_tag.sensor[0].last_sample_humidity = 50.0f;
	g_app_data.ble_tag.sensor[0].addr[0] = 0xAD;
	g_app_data.ble_tag.sensor[1].last_sample_temperature = 10.25f;
	g_app_data.ble_tag.sensor[1].last_sample_humidity = 55.0f;
	g_app_data.ble_tag.sensor[1].addr[0] = 0xAD;
	CTR_TEST_LRW_JSON_BLE_TAGS(data);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	g_app_data.trigger[0] = malloc(sizeof(struct app_data_trigger));
	memset(g_app_data.trigger[0], 0, sizeof(struct app_data_trigger));

	g_app_data.counter[1] = malloc(sizeof(struct app_data_counter));
	memset(g_app_data.counter[1], 0, sizeof(struct app_data_counter));

	g_app_data.voltage[2] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.voltage[2], 0, sizeof(struct app_data_analog));

	g_app_data.current[3] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.current[3], 0, sizeof(struct app_data_analog));

	g_app_data.trigger[0]->trigger_active = true;
	g_app_data.trigger[0]->event_count = 1;
	g_app_data.trigger[0]->events[0].is_active = true;

	g_app_data.counter[1]->value = 123456;
	g_app_data.counter[1]->delta = 7890;
	g_app_data.voltage[2]->last_sample = 28.25f;
	g_app_data.current[3]->last_sample = 23.25f;

	CTR_TEST_LRW_JSON_X0_A_INPUTS(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST(app_control_lrw, test_negative)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "data", data);

#if defined(CONFIG_CTR_BATT)
	g_app_data.system_voltage_rest = 3250.f;
	g_app_data.system_voltage_load = 3125.f;
	g_app_data.system_current_load = 15;
	CTR_TEST_LRW_JSON_BATT(data);
#endif

#if defined(CONFIG_CTR_ACCEL)
	g_app_data.accel_orientation = 3;
	CTR_TEST_LRW_JSON_ORIENTATION(data);
#endif

#if defined(CONFIG_CTR_THERM)
	g_app_data.therm_temperature = -23.25f;
	CTR_TEST_LRW_JSON_THERM(data);
#endif

// Hygro
#if defined(FEATURE_HARDWARE_CHESTER_S2)
	g_app_data.hygro.last_sample_temperature = -21.25f;
	g_app_data.hygro.last_sample_humidity = 45;
	CTR_TEST_LRW_JSON_HYGRO(data);
#endif

// 1-Wire
#if defined(FEATURE_SUBSYSTEM_DS18B20)
	g_app_data.w1_therm.sensor[0].last_sample_temperature = -18.25f;
	g_app_data.w1_therm.sensor[0].serial_number = 0x123456789ABCDEF0;
	g_app_data.w1_therm.sensor[1].last_sample_temperature = -20.25f;
	g_app_data.w1_therm.sensor[1].serial_number = 0x123456789ABCDEF1;
	CTR_TEST_LRW_JSON_W1_THERMOMETERS(data);
#endif

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	// BLE Tags
	g_app_data.ble_tag.sensor[0].last_sample_temperature = -5.25f;
	g_app_data.ble_tag.sensor[0].last_sample_humidity = 50.0f;
	g_app_data.ble_tag.sensor[0].addr[0] = 0xAD;
	g_app_data.ble_tag.sensor[1].last_sample_temperature = -10.25f;
	g_app_data.ble_tag.sensor[1].last_sample_humidity = 55.0f;
	g_app_data.ble_tag.sensor[1].addr[0] = 0xAD;
	CTR_TEST_LRW_JSON_BLE_TAGS(data);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	g_app_data.trigger[0] = malloc(sizeof(struct app_data_trigger));
	memset(g_app_data.trigger[0], 0, sizeof(struct app_data_trigger));

	g_app_data.counter[1] = malloc(sizeof(struct app_data_counter));
	memset(g_app_data.counter[1], 0, sizeof(struct app_data_counter));

	g_app_data.voltage[2] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.voltage[2], 0, sizeof(struct app_data_analog));

	g_app_data.current[3] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.current[3], 0, sizeof(struct app_data_analog));

	g_app_data.trigger[0]->trigger_active = true;
	g_app_data.trigger[0]->event_count = 1;
	g_app_data.trigger[0]->events[0].is_active = true;

	g_app_data.counter[1]->value = 123456;
	g_app_data.counter[1]->delta = 7890;
	g_app_data.voltage[2]->last_sample = 28.25f;
	g_app_data.current[3]->last_sample = 23.25f;

	CTR_TEST_LRW_JSON_X0_A_INPUTS(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST(app_control_lrw, test_null)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "data", data);

#if defined(CONFIG_CTR_BATT)
	g_app_data.system_voltage_rest = NAN;
	g_app_data.system_voltage_load = NAN;
	g_app_data.system_current_load = NAN;
	CTR_TEST_LRW_JSON_BATT(data);
#endif

#if defined(CONFIG_CTR_ACCEL)
	g_app_data.accel_orientation = UINT8_MAX;
	CTR_TEST_LRW_JSON_ORIENTATION(data);
#endif

#if defined(CONFIG_CTR_THERM)
	g_app_data.therm_temperature = NAN;
	CTR_TEST_LRW_JSON_THERM(data);
#endif

// Hygro
#if defined(FEATURE_HARDWARE_CHESTER_S2)
	g_app_data.hygro.last_sample_temperature = NAN;
	g_app_data.hygro.last_sample_humidity = NAN;
	CTR_TEST_LRW_JSON_HYGRO(data);
#endif

// 1-Wire
#if defined(FEATURE_SUBSYSTEM_DS18B20)
	g_app_data.w1_therm.sensor[0].last_sample_temperature = NAN;
	g_app_data.w1_therm.sensor[0].serial_number = 0x123456789ABCDEF0;
	g_app_data.w1_therm.sensor[1].last_sample_temperature = NAN;
	g_app_data.w1_therm.sensor[1].serial_number = 0x123456789ABCDEF1;
	CTR_TEST_LRW_JSON_W1_THERMOMETERS(data);
#endif

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	// BLE Tags
	g_app_data.ble_tag.sensor[0].last_sample_temperature = NAN;
	g_app_data.ble_tag.sensor[0].last_sample_humidity = NAN;
	g_app_data.ble_tag.sensor[0].addr[0] = 0xAD;
	g_app_data.ble_tag.sensor[1].last_sample_temperature = NAN;
	g_app_data.ble_tag.sensor[1].last_sample_humidity = NAN;
	g_app_data.ble_tag.sensor[1].addr[0] = 0xAD;
	CTR_TEST_LRW_JSON_BLE_TAGS(data);
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	g_app_data.trigger[0] = malloc(sizeof(struct app_data_trigger));
	memset(g_app_data.trigger[0], 0, sizeof(struct app_data_trigger));

	g_app_data.counter[1] = malloc(sizeof(struct app_data_counter));
	memset(g_app_data.counter[1], 0, sizeof(struct app_data_counter));

	g_app_data.voltage[2] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.voltage[2], 0, sizeof(struct app_data_analog));

	g_app_data.current[3] = malloc(sizeof(struct app_data_analog));
	memset(g_app_data.current[3], 0, sizeof(struct app_data_analog));

	g_app_data.trigger[0]->trigger_active = false;
	g_app_data.trigger[0]->event_count = 1;
	g_app_data.trigger[0]->events[0].is_active = false;

	g_app_data.counter[1]->value = UINT32_MAX;
	g_app_data.counter[1]->delta = UINT16_MAX;
	g_app_data.voltage[2]->last_sample = NAN;
	g_app_data.current[3]->last_sample = NAN;

	CTR_TEST_LRW_JSON_X0_A_INPUTS(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST_SUITE(app_control_lrw, NULL, NULL, NULL, NULL, NULL);
