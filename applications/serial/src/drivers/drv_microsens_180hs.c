/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_microsens_180hs.c
 * @brief MicroSENS 180-HS CO2 sensor driver implementation
 *
 * This driver communicates with MicroSENS 180-HS via RS-232 (CHESTER-X12 module).
 * Protocol: Binary command/response with STX/ETX framing.
 */

#include "drv_microsens_180hs.h"
#include "drv_microsens_180hs_proto.h"
#include "drv_interface.h"
#include "../app_device.h"
#include "../app_config.h"
#include "../app_serial.h"

#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <math.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(microsens, LOG_LEVEL_INF);

/* MicroSENS protocol constants */
/* Frame constants and the frame/response parsers live in drv_microsens_180hs_proto.{c,h}
 * so they can be unit tested on the host.
 */
#define STX DRV_MICROSENS_180HS_STX
#define ETX DRV_MICROSENS_180HS_ETX

/* MicroSENS commands (hex) */
#define CMD_GET_MEASUREMENT    "1100"
#define CMD_ZERO_CALIBRATION   "1203"
#define CMD_SENSOR_RESET       "1908"

/* Zero calibration defaults */
#define ZERO_CAL_DEFAULT_VALUE 40   /* 0.04 Vol.-% (400 ppm background CO2) */
#define ZERO_CAL_MIN_VALUE     0    /* 0 Vol.-% */
#define ZERO_CAL_MAX_VALUE     500  /* 0.5 Vol.-% */

/* Timeouts */
#define RESPONSE_TIMEOUT_MS     1000
#define MEASURE_DELAY_MS        150
#define CALIBRATION_WAIT_MS     2000
#define RESET_WAIT_MS           3000

/* MicroSENS error codes */
#define MICROSENS_ERROR_SENSOR   -1000
#define MICROSENS_ERROR_COMM     -2000
#define MICROSENS_ERROR_SHUTDOWN -3000

/**
 * @brief Helper function to set measurement error state
 */
static void set_measurement_error(struct app_data_microsens *data)
{
	data->co2_percent = NAN;
	data->temperature_c = NAN;
	data->pressure_mbar = NAN;
	data->valid = false;
	data->error_count++;
}

/**
 * @brief Send command to MicroSENS sensor
 *
 * Builds STX/ETX frame and sends via serial.
 *
 * @param cmd ASCII command string (e.g., "1100")
 * @return 0 on success, negative error code on failure
 */
static int send_command(const char *cmd)
{
	size_t cmd_len = strlen(cmd);
	uint8_t frame[64];
	size_t idx = 0;

	if (cmd_len > sizeof(frame) - 2) {
		LOG_ERR("Command too long");
		return -EINVAL;
	}

	/* Build frame: STX <ASCII_COMMAND> ETX */
	frame[idx++] = STX;
	memcpy(&frame[idx], cmd, cmd_len);
	idx += cmd_len;
	frame[idx++] = ETX;

	LOG_HEXDUMP_DBG(frame, idx, "TX frame");

	/* Flush RX buffer before sending */
	app_serial_flush_rx();

	/* Send via serial */
	int ret = app_serial_send(frame, idx);
	if (ret < 0) {
		LOG_ERR("app_serial_send failed: %d", ret);
		return ret;
	}

	LOG_INF("Sent command '%s' (%zu bytes)", cmd, idx);

	return 0;
}

/**
 * @brief Receive STX/ETX framed response from sensor
 *
 * Uses app_serial_receive_timeout() which has inter-character timeout
 * to wait for complete response.
 */
static int receive_response(uint8_t *data, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
	uint8_t buf[128];
	size_t buf_len = 0;

	/* app_serial_receive_timeout has inter-character timeout (100ms) */
	int ret = app_serial_receive_timeout(buf, sizeof(buf), &buf_len, timeout_ms);
	if (ret) {
		LOG_ERR("No response received (timeout)");
		return ret;
	}

	LOG_INF("Received %zu bytes", buf_len);
	LOG_HEXDUMP_DBG(buf, buf_len, "RX buffer");

	/* Pull the payload out of the STX...ETX frame (pure codec). */
	ret = drv_microsens_180hs_extract_frame(buf, buf_len, data, max_len, out_len);
	if (ret) {
		LOG_ERR("Invalid response frame: %d", ret);
		LOG_HEXDUMP_WRN(buf, buf_len, "Invalid response");
		return ret;
	}

	return 0;
}

/**
 * @brief Drop the shared UART bridge back to its idle (suspended) state
 *
 * app_serial_send() resumes the bridge on demand but never suspends it again, so a
 * polled sensor like this must release it after each transaction or the bridge would
 * stay at its ~5.4 mA running current until the next reboot. The suspend drains any
 * pending TX first and is idempotent, so it is safe on every exit path.
 */
static void serial_suspend(void)
{
	int ret = app_serial_pm(false);
	if (ret) {
		LOG_WRN("Could not suspend UART bridge after transaction: %d", ret);
	}
}

/*
 * The driver callbacks run under the slot lock (app_device.c), so a calibration or reset
 * never overlaps a measurement.
 */

static int init(struct app_driver_slot *slot)
{
	struct app_data_microsens *data = slot->ctx;

	LOG_INF("Initializing MicroSENS 180-HS driver");

	/* Initialize data structure */
	data->co2_percent = NAN;
	data->temperature_c = NAN;
	data->pressure_mbar = NAN;
	data->valid = false;
	data->last_sample = 0;
	data->error_count = 0;

	LOG_INF("MicroSENS 180-HS driver initialized");

	return 0;
}

static int sample(struct app_driver_slot *slot)
{
	struct app_data_microsens *data = slot->ctx;
	int ret;
	uint8_t response[128];
	size_t response_len;
	uint32_t sensor_id = 0;
	uint32_t timestamp = 0;
	int32_t co2_raw = 0;
	int16_t temp_raw = 0;
	int16_t pressure_raw = 0;

	LOG_INF("Sampling MicroSENS 180-HS sensor");

	/* Send measurement command */
	ret = send_command(CMD_GET_MEASUREMENT);
	if (ret) {
		LOG_ERR("Failed to send measurement command: %d", ret);
		set_measurement_error(data);
		goto out;
	}

	/* Sensor needs time to measure before responding */
	k_sleep(K_MSEC(MEASURE_DELAY_MS));

	/* Wait for response */
	ret = receive_response(response, sizeof(response) - 1, &response_len, RESPONSE_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to receive response: %d", ret);
		set_measurement_error(data);
		goto out;
	}

	/* Log raw response */
	LOG_INF("Raw response (%zu bytes): '%s'", response_len, (char *)response);

	/* Parse response */
	ret = drv_microsens_180hs_parse_measurement((char *)response, &sensor_id, &timestamp, &co2_raw,
					 &temp_raw, &pressure_raw);
	if (ret) {
		LOG_ERR("Failed to parse response: %d", ret);
		set_measurement_error(data);
		goto out;
	}

	/* Check for error values */
	if (co2_raw == MICROSENS_ERROR_SENSOR) {
		LOG_ERR("Sensor error: CO2 = %d (sensor malfunction)", co2_raw);
		set_measurement_error(data);
		ret = -EIO;
		goto out;
	} else if (co2_raw == MICROSENS_ERROR_COMM) {
		LOG_ERR("Sensor error: CO2 = %d (communication error)", co2_raw);
		set_measurement_error(data);
		ret = -EIO;
		goto out;
	} else if (co2_raw == MICROSENS_ERROR_SHUTDOWN) {
		LOG_ERR("Sensor error: CO2 = %d (automatic shutdown, temp > 85C)", co2_raw);
		set_measurement_error(data);
		ret = -EIO;
		goto out;
	}

	/* Convert to actual values */
	float co2_percent = co2_raw / 1000.0f;     /* Vol.-% * 1000 -> Vol.-% */
	float temp_celsius = temp_raw / 10.0f;     /* C * 10 -> C */
	float pressure_mbar = (float)pressure_raw; /* mbar */

	/* Validate ranges */
	if (temp_celsius < -40.0f || temp_celsius > 85.0f) {
		LOG_WRN("Temperature out of range: %.1f C", (double)temp_celsius);
	}

	if (pressure_mbar < 300.0f || pressure_mbar > 1100.0f) {
		LOG_WRN("Pressure out of range: %.0f mbar", (double)pressure_mbar);
	}

	if (co2_percent < 0.0f || co2_percent > 100.0f) {
		LOG_WRN("CO2 out of range: %.3f%%", (double)co2_percent);
	}

	LOG_INF("Sensor ID: %u", sensor_id);
	LOG_INF("CO2: %.3f%% (raw: %d)", (double)co2_percent, co2_raw);
	LOG_INF("Temperature: %.1f C (raw: %d)", (double)temp_celsius, temp_raw);
	LOG_INF("Pressure: %.0f mbar (raw: %d)", (double)pressure_mbar, pressure_raw);

	data->co2_percent = co2_percent;
	data->temperature_c = temp_celsius;
	data->pressure_mbar = pressure_mbar;
	data->valid = true;
	data->last_sample = k_uptime_get_32();
	data->error_count = 0;

	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct microsens_sample rec = {
		.timestamp = ts,
		.co2_percent = co2_percent,
		.temperature_c = temp_celsius,
		.pressure_mbar = pressure_mbar,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("MicroSENS sampling complete");

	ret = 0;

out:
	/* Release the bridge back to idle current now the transaction is done. */
	serial_suspend();

	return ret;
}

static int calibrate(struct app_driver_slot *slot, enum app_device_calibration cal_type, int value)
{
	int ret;
	uint8_t response[16];
	size_t response_len;
	char cmd[16];

	ARG_UNUSED(slot);

	if (cal_type == APP_DEVICE_CAL_ZERO) {
		/* Use default value if not specified (0 means use default) */
		int cal_value = (value > 0) ? value : ZERO_CAL_DEFAULT_VALUE;

		/* Validate range */
		if (cal_value < ZERO_CAL_MIN_VALUE || cal_value > ZERO_CAL_MAX_VALUE) {
			LOG_ERR("Zero calibration value out of range: %d (valid: %d-%d)",
				cal_value, ZERO_CAL_MIN_VALUE, ZERO_CAL_MAX_VALUE);
			return -EINVAL;
		}

		LOG_INF("Performing zero point calibration to %.3f Vol.-%% (raw: %d)",
			(double)cal_value / 1000.0, cal_value);

		/* Build command: "1203" + value (e.g., "120340" for 0.04%) */
		snprintf(cmd, sizeof(cmd), "%s%d", CMD_ZERO_CALIBRATION, cal_value);

		ret = send_command(cmd);
		if (ret) {
			LOG_ERR("Failed to send calibration command: %d", ret);
			serial_suspend();
			return ret;
		}

		/* Wait for acknowledgment (response: 0=success, 1=failed) */
		ret = receive_response(response, sizeof(response) - 1, &response_len,
				       RESPONSE_TIMEOUT_MS);
		if (ret) {
			LOG_WRN("No acknowledgment received: %d", ret);
		} else {
			LOG_DBG("Calibration response: '%s'", response);
			if (response[0] == '1') {
				LOG_ERR("Calibration failed (sensor returned error)");
				serial_suspend();
				return -EIO;
			}
		}

		/* Sensor needs time to complete calibration */
		k_sleep(K_MSEC(CALIBRATION_WAIT_MS));

		serial_suspend();

		LOG_INF("Zero point calibration complete");

		return 0;
	} else if (cal_type == APP_DEVICE_CAL_SPAN) {
		LOG_WRN("Span calibration not supported");
		return -ENOTSUP;
	}

	return -EINVAL;
}

static int reset(struct app_driver_slot *slot)
{
	int ret;

	ARG_UNUSED(slot);

	LOG_INF("Performing sensor reset");

	ret = send_command(CMD_SENSOR_RESET);
	if (ret) {
		LOG_ERR("Failed to send reset command: %d", ret);
		serial_suspend();
		return ret;
	}

	/* Sensor reset command (1908) has no response - just wait for reinitialization */
	k_sleep(K_MSEC(RESET_WAIT_MS));

	serial_suspend();

	LOG_INF("Sensor reset complete");

	return 0;
}

const struct app_data_microsens *microsens_get_data(const struct app_driver_slot *slot)
{
	return slot->ctx;
}

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct app_data_microsens *data = slot->ctx;

	shell_print(shell, "[%d] MicroSens 180-HS (RS232):", slot->idx);
	shell_print(shell, "  CO2:      %.3f Vol.-%%", (double)data->co2_percent);
	shell_print(shell, "  Temp:     %.1f C", (double)data->temperature_c);
	shell_print(shell, "  Pressure: %.0f mbar", (double)data->pressure_mbar);
	shell_print(shell, "  Valid:    %s", data->valid ? "yes" : "no");
}

/* Driver registration */
const struct app_device_driver microsens_driver = {
	.name = "microsens",
	.type = APP_DEVICE_TYPE_MICROSENS_180HS,
	.transport = APP_DEVICE_TRANSPORT_UART,
	.max_instances = 1, /* RS-232 is point-to-point, one UART */
	.ctx_size = sizeof(struct app_data_microsens),
	.rec_size = sizeof(struct microsens_sample),
	.init = init,
	.sample = sample,
	.calibrate = calibrate,
	.reset = reset,
	.print_data = print_data,
};

/*
 * Shell commands
 */

static int find_microsens_device(void)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type == APP_DEVICE_TYPE_MICROSENS_180HS) {
			return i;
		}
	}
	return -1;
}

static int cmd_microsens_sample(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_microsens_device();
	if (device_idx < 0) {
		shell_error(shell, "MicroSENS 180-HS device not configured");
		return -ENODEV;
	}

	shell_print(shell, "Sampling MicroSENS 180-HS sensor...");

	int ret = app_device_sample(device_idx);
	if (ret) {
		shell_error(shell, "Sampling failed: %d", ret);
		return ret;
	}

	struct app_driver_slot *slot = app_device_get_slot(device_idx);

	if (slot) {
		app_driver_slot_lock(slot);
		print_data(shell, slot);
		app_driver_slot_unlock(slot);
	}

	return 0;
}

static int cmd_microsens_calibrate(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_microsens_device();
	if (device_idx < 0) {
		shell_error(shell, "MicroSENS 180-HS device not configured");
		return -ENODEV;
	}

	if (argc < 2) {
		shell_error(shell, "Usage: device microsens_180hs calibrate zero [value]");
		shell_error(shell, "  value: 0-500 (Vol.-%% * 1000), default: 40 (0.04%%)");
		return -EINVAL;
	}

	if (strcmp(argv[1], "zero") == 0) {
		int cal_value = 0; /* 0 = use default (40) */

		/* Parse optional concentration parameter */
		if (argc >= 3) {
			cal_value = atoi(argv[2]);
			if (cal_value < ZERO_CAL_MIN_VALUE || cal_value > ZERO_CAL_MAX_VALUE) {
				shell_error(shell, "Value out of range: %d (valid: %d-%d)",
					    cal_value, ZERO_CAL_MIN_VALUE, ZERO_CAL_MAX_VALUE);
				return -EINVAL;
			}
			shell_print(shell, "Zero calibration to %.3f Vol.-%% (raw: %d)",
				    (double)cal_value / 1000.0, cal_value);
		} else {
			shell_print(shell, "Zero calibration to default %.3f Vol.%%",
				    (double)ZERO_CAL_DEFAULT_VALUE / 1000.0);
		}

		shell_print(shell, "Make sure sensor is in known CO2 atmosphere!");

		int ret = app_device_calibrate(device_idx, APP_DEVICE_CAL_ZERO, cal_value);
		if (ret) {
			shell_error(shell, "Calibration failed: %d", ret);
			return ret;
		}

		shell_print(shell, "Zero point calibration complete");
	} else if (strcmp(argv[1], "span") == 0) {
		shell_error(shell, "Span calibration not supported for MicroSENS");
		return -ENOTSUP;
	} else {
		shell_error(shell, "Invalid calibration type: %s", argv[1]);
		return -EINVAL;
	}

	return 0;
}

static int cmd_microsens_reset(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_microsens_device();
	if (device_idx < 0) {
		shell_error(shell, "MicroSENS 180-HS device not configured");
		return -ENODEV;
	}

	shell_print(shell, "Performing sensor reset...");

	int ret = app_device_reset(device_idx);
	if (ret) {
		shell_error(shell, "Reset failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Sensor reset complete");

	return 0;
}

/* clang-format off */

/* MicroSENS subcommands - exported for app_device.c
 *
 * Note: We define the array manually (not using SHELL_STATIC_SUBCMD_SET_CREATE)
 * because we need to export it for use in app_device.c. The macro creates
 * static variables which cannot be exported.
 */
const struct shell_static_entry microsens_shell_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Sample MicroSENS 180-HS sensor", cmd_microsens_sample, 1, 0),
	SHELL_CMD_ARG(calibrate, NULL, "Calibrate sensor: zero [value]", cmd_microsens_calibrate, 2, 1),
	SHELL_CMD_ARG(reset, NULL, "Soft reset MicroSENS sensor", cmd_microsens_reset, 1, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry microsens_shell_cmds = {
	.entry = microsens_shell_subcmds
};

/* clang-format on */
