/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/** @file
 *  @brief Unit tests for the pure MicroSENS 180-HS frame/response parser.
 *
 *  Runs on native_sim, no UART involved. Covers STX/ETX payload extraction (including
 *  surrounding noise and the too-long / missing-delimiter cases) and the sscanf-based
 *  measurement field parsing, including the sensor's negative error codes.
 */

#include <zephyr/ztest.h>

#include <errno.h>
#include <string.h>

#include <drv_microsens_180hs_proto.h>

#define STX DRV_MICROSENS_180HS_STX
#define ETX DRV_MICROSENS_180HS_ETX

/* ---- extract_frame ------------------------------------------------------------- */

ZTEST(microsens_proto, test_extract_ok)
{
	const uint8_t buf[] = {STX, '1', '2', '3', ETX};
	uint8_t out[16];
	size_t out_len = 0;

	zassert_ok(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, sizeof(out) - 1,
						     &out_len));
	zassert_equal(out_len, 3);
	zassert_mem_equal(out, "123", 3);
	zassert_equal(out[3], '\0', "must be NUL terminated");
}

ZTEST(microsens_proto, test_extract_ignores_surrounding_noise)
{
	/* Junk before STX and after ETX must be discarded. */
	const uint8_t buf[] = {0xFF, 0xFF, STX, 'A', 'B', ETX, 0x00, 0x99};
	uint8_t out[16];
	size_t out_len = 0;

	zassert_ok(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, sizeof(out) - 1,
						     &out_len));
	zassert_equal(out_len, 2);
	zassert_mem_equal(out, "AB", 2);
}

ZTEST(microsens_proto, test_extract_missing_stx)
{
	const uint8_t buf[] = {'1', '2', '3', ETX};
	uint8_t out[16];
	size_t out_len = 0;

	zassert_equal(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, sizeof(out) - 1,
							&out_len),
		      -EIO);
}

ZTEST(microsens_proto, test_extract_missing_etx)
{
	const uint8_t buf[] = {STX, '1', '2', '3'};
	uint8_t out[16];
	size_t out_len = 0;

	zassert_equal(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, sizeof(out) - 1,
							&out_len),
		      -EIO);
}

ZTEST(microsens_proto, test_extract_too_long)
{
	const uint8_t buf[] = {STX, 'A', 'B', 'C', 'D', ETX};
	uint8_t out[8];
	size_t out_len = 0;

	/* Payload is 4 bytes; cap at 2 -> must refuse rather than overflow. */
	zassert_equal(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, 2, &out_len),
		      -ENOMEM);
}

ZTEST(microsens_proto, test_extract_rejects_null)
{
	const uint8_t buf[] = {STX, 'A', ETX};
	uint8_t out[8];
	size_t out_len = 0;

	zassert_equal(drv_microsens_180hs_extract_frame(NULL, 3, out, 7, &out_len), -EINVAL);
	zassert_equal(drv_microsens_180hs_extract_frame(buf, sizeof(buf), NULL, 7, &out_len),
		      -EINVAL);
	zassert_equal(drv_microsens_180hs_extract_frame(buf, sizeof(buf), out, 7, NULL), -EINVAL);
}

/* ---- parse_measurement --------------------------------------------------------- */

ZTEST(microsens_proto, test_parse_measurement_ok)
{
	uint32_t sensor_id = 0, timestamp = 0;
	int32_t co2 = 0;
	int16_t temp = 0, pressure = 0;

	zassert_ok(drv_microsens_180hs_parse_measurement("123 456789 42000 235 1013", &sensor_id,
							 &timestamp, &co2, &temp, &pressure));
	zassert_equal(sensor_id, 123);
	zassert_equal(timestamp, 456789);
	zassert_equal(co2, 42000);   /* 42.000 Vol.-% *1000 */
	zassert_equal(temp, 235);    /* 23.5 C *10 */
	zassert_equal(pressure, 1013);
}

ZTEST(microsens_proto, test_parse_measurement_negative_error_code)
{
	uint32_t sensor_id = 0, timestamp = 0;
	int32_t co2 = 0;
	int16_t temp = 0, pressure = 0;

	/* co2 = -1000 is the sensor-malfunction code; must survive as a signed value. */
	zassert_ok(drv_microsens_180hs_parse_measurement("7 100 -1000 -50 980", &sensor_id,
							 &timestamp, &co2, &temp, &pressure));
	zassert_equal(co2, -1000);
	zassert_equal(temp, -50);
}

ZTEST(microsens_proto, test_parse_measurement_too_few_fields)
{
	uint32_t sensor_id = 0, timestamp = 0;
	int32_t co2 = 0;
	int16_t temp = 0, pressure = 0;

	zassert_equal(drv_microsens_180hs_parse_measurement("123 456 42000", &sensor_id,
							    &timestamp, &co2, &temp, &pressure),
		      -EINVAL);
}

ZTEST(microsens_proto, test_parse_measurement_rejects_null)
{
	uint32_t sensor_id = 0, timestamp = 0;
	int32_t co2 = 0;
	int16_t temp = 0, pressure = 0;

	zassert_equal(drv_microsens_180hs_parse_measurement(NULL, &sensor_id, &timestamp, &co2,
							    &temp, &pressure),
		      -EINVAL);
	zassert_equal(drv_microsens_180hs_parse_measurement("1 2 3 4 5", NULL, &timestamp, &co2,
							    &temp, &pressure),
		      -EINVAL);
}

ZTEST_SUITE(microsens_proto, NULL, NULL, NULL, NULL, NULL);
