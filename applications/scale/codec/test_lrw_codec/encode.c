/** @file
 *  @brief scale LRW codec test suite
 */

#include <app_lrw.h>
#include <app_data.h>
#include <app_config.h>

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

/* Mock g_app_config for test - app_lrw.c uses this for channel active flags */
struct app_config g_app_config = {
	.channel_a1_active = true,
	.channel_a2_active = true,
	.channel_b1_active = true,
	.channel_b2_active = true,
};

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

void add_scale_json(cJSON *data)
{
	cJSON *scale = cJSON_CreateObject();

	cJSON_AddBoolToObject(scale, "channel_a1_active", g_app_config.channel_a1_active);
	cJSON_AddBoolToObject(scale, "channel_a2_active", g_app_config.channel_a2_active);
	cJSON_AddBoolToObject(scale, "channel_b1_active", g_app_config.channel_b1_active);
	cJSON_AddBoolToObject(scale, "channel_b2_active", g_app_config.channel_b2_active);

	if (g_app_data.weight_measurement_count > 0) {
		int idx = g_app_data.weight_measurement_count - 1;
		int32_t a1 = g_app_data.weight_measurements[idx].a1_raw;
		int32_t a2 = g_app_data.weight_measurements[idx].a2_raw;
		int32_t b1 = g_app_data.weight_measurements[idx].b1_raw;
		int32_t b2 = g_app_data.weight_measurements[idx].b2_raw;

		if (a1 == INT32_MAX) {
			cJSON_AddNullToObject(scale, "raw_a1");
		} else {
			cJSON_AddNumberToObject(scale, "raw_a1", a1);
		}
		if (a2 == INT32_MAX) {
			cJSON_AddNullToObject(scale, "raw_a2");
		} else {
			cJSON_AddNumberToObject(scale, "raw_a2", a2);
		}
		if (b1 == INT32_MAX) {
			cJSON_AddNullToObject(scale, "raw_b1");
		} else {
			cJSON_AddNumberToObject(scale, "raw_b1", b1);
		}
		if (b2 == INT32_MAX) {
			cJSON_AddNullToObject(scale, "raw_b2");
		} else {
			cJSON_AddNumberToObject(scale, "raw_b2", b2);
		}
	} else {
		cJSON_AddNullToObject(scale, "raw_a1");
		cJSON_AddNullToObject(scale, "raw_a2");
		cJSON_AddNullToObject(scale, "raw_b1");
		cJSON_AddNullToObject(scale, "raw_b2");
	}

	cJSON_AddItemToObject(data, "scale", scale);
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

	CTR_BUF_DEFINE_STATIC(lrw_buf, 64);
	app_lrw_encode(&lrw_buf);
	char filename_b64[255];
	snprintf(filename_b64, sizeof(filename_b64), "data/%s.b64", filename);
	write_base64_file(filename_b64, &lrw_buf);
}

ZTEST(app_scale_lrw, test_positive)
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

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	g_app_config.channel_a1_active = true;
	g_app_config.channel_a2_active = true;
	g_app_config.channel_b1_active = true;
	g_app_config.channel_b2_active = true;

	g_app_data.weight_measurement_count = 1;
	g_app_data.weight_measurements[0].a1_raw = 123456;
	g_app_data.weight_measurements[0].a2_raw = 234567;
	g_app_data.weight_measurements[0].b1_raw = 345678;
	g_app_data.weight_measurements[0].b2_raw = 456789;

	add_scale_json(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST(app_scale_lrw, test_negative)
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

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	g_app_config.channel_a1_active = true;
	g_app_config.channel_a2_active = false;
	g_app_config.channel_b1_active = true;
	g_app_config.channel_b2_active = false;

	g_app_data.weight_measurement_count = 1;
	g_app_data.weight_measurements[0].a1_raw = -123456;
	g_app_data.weight_measurements[0].a2_raw = -234567;
	g_app_data.weight_measurements[0].b1_raw = -345678;
	g_app_data.weight_measurements[0].b2_raw = -456789;

	add_scale_json(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST(app_scale_lrw, test_null)
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

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	g_app_config.channel_a1_active = false;
	g_app_config.channel_a2_active = false;
	g_app_config.channel_b1_active = false;
	g_app_config.channel_b2_active = false;

	g_app_data.weight_measurement_count = 1;
	g_app_data.weight_measurements[0].a1_raw = INT32_MAX;
	g_app_data.weight_measurements[0].a2_raw = INT32_MAX;
	g_app_data.weight_measurements[0].b1_raw = INT32_MAX;
	g_app_data.weight_measurements[0].b2_raw = INT32_MAX;

	add_scale_json(data);
#endif

	create_output_files(__func__, root);

	cJSON_Delete(root);
}

ZTEST_SUITE(app_scale_lrw, NULL, NULL, NULL, NULL, NULL);
