/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_piketronic_rppu.c
 * @brief Piketronic RPP-U radon probe driver (modified UART-only variant)
 *
 * The stock RPP-U is a USB device. This driver targets the CUSTOM-MODIFIED unit in which
 * Piketronic removed the internal UART-USB bridge, so the USB-B connector carries a plain
 * 3.3 V UART:
 *
 *     D+ (green) = TX_out (probe -> CHESTER RX, X2 pin 3)
 *     D- (white) = RX_in  (CHESTER TX -> probe, X2 pin 4)
 *     GND (black)                                    (X2 pin 1)
 *     VCC (red) is charging only - do NOT connect to X2
 *
 * Line settings are 115200 8N1; the probe is the slave. Configure the app accordingly:
 *
 *     app config serial-mode uart
 *     app config serial-baudrate 115200
 *     app config serial-parity none
 *     app config device-0 "piketronic_u"
 *
 * WAKE BEHAVIOUR (measured on S/N 24123, and the whole reason this driver exists):
 * the probe answers only for roughly 15-30 s after the UART bridge is initialized, then
 * goes deaf. The vendor note says RX_in must be parked at 0 while idle and held at 1 for
 * 2 s before a command - but a break low->high edge alone does NOT re-arm it. Re-running
 * uart_configure() does. app_serial_wake() performs exactly that sequence, so every
 * transaction here is preceded by it. Without this, reads succeed once per boot only.
 *
 * Frame format:  @ | nB_H | nB_L | Cmd | data... | CSUM
 *   nB   = number of bytes after nB excluding CSUM (i.e. Cmd + data)
 *   CSUM = low byte of the 8-bit sum of every byte except the leading '@'
 * All multi-byte values are MSB first.
 */

#include "drv_piketronic_rppu.h"
#include "drv_piketronic_rppu_proto.h"
#include "drv_interface.h"
#include "../app_config.h"
#include "../app_device.h"
#include "../app_serial.h"
#include "../app_work.h"

#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_piketronic_rppu, LOG_LEVEL_INF);

/* Frame layout and codec live in drv_piketronic_rppu_proto.{c,h} so they can be unit
 * tested on the host. Short local aliases keep the transport code below readable.
 */
#define FRAME_START	 DRV_PIKETRONIC_RPPU_FRAME_START
#define DATA_PAYLOAD_LEN DRV_PIKETRONIC_RPPU_DATA_PAYLOAD_LEN
#define DATA_FRAME_LEN	 DRV_PIKETRONIC_RPPU_DATA_FRAME_LEN
#define SERIAL_ASCII_LEN DRV_PIKETRONIC_RPPU_SERIAL_ASCII_LEN
#define SERIAL_FRAME_LEN DRV_PIKETRONIC_RPPU_SERIAL_FRAME_LEN

/* Requests, precomputed (checksums verified against the vendor examples) */
static const uint8_t CMD_DATA[] = {0x40, 0x00, 0x01, 0x44, 0x45};   /* 'D' */
static const uint8_t CMD_SERIAL[] = {0x40, 0x00, 0x01, 0x56, 0x57}; /* 'V' */

/* Park the line low this long so the probe drops into power-save, then settle high.
 * The vendor asks for 2 s at logic 1 before the command; 2500 ms gives margin.
 */
#define WAKE_LOW_MS  250
#define WAKE_HIGH_MS 2500

#define RESPONSE_TIMEOUT_MS 4000

/* Low-accumulator alert.
 *
 * The probe powers itself off below 3300 mV, and a report interval is 30 minutes by default,
 * so on the normal cadence a remote installation can lose up to half an hour of warning -
 * and in the worst case the first sign is simply that the readings stopped. Crossing the
 * configured threshold therefore triggers an uplink straight away, carrying the voltage that
 * app_cbor/app_lrw now encode.
 *
 * Edge-triggered, not level-triggered: it fires on the downward crossing only, and re-arms
 * once the accumulator recovers past the threshold plus ALERT_HYSTERESIS_MV. Without the
 * hysteresis a probe sitting right on the threshold would re-arm and re-alert on alternating
 * samples. ALERT_MIN_INTERVAL_S is a second, independent floor: even a genuine repeated
 * crossing cannot uplink more often than this, which matters on LoRaWAN duty cycle.
 */
#define ALERT_HYSTERESIS_MV   50
#define ALERT_MIN_INTERVAL_S  3600

/* Slot state (slot->ctx): the latest reading and the low-accumulator alert. The samples for
 * the CBOR/LRW uplink are the records of the slot.
 */
struct rppu_ctx {
	struct drv_piketronic_rppu_data data;
	bool alert_armed;          /* false once fired, until the accumulator recovers */
	int64_t alert_last_uptime; /* s, -1 = never */
};

/**
 * @brief Wake the probe, send @p tx, and collect exactly @p expect_len bytes.
 */
static int transact(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t expect_len)
{
	int ret;

	/* Resume the UART bridge. In transparent mode app_serial_init() leaves the SC16IS740
	 * powered permanently (unlike the Modbus path, which brackets each transaction with
	 * PM suspend/resume) - measured as ~6 mA of continuous draw, which would flatten the
	 * LS 26500 in weeks. We keep it suspended while idle and resume only to talk.
	 * Convenient side effect: a PM resume also re-arms the probe, same as uart_configure().
	 */
	ret = app_serial_pm(true);
	if (ret) {
		LOG_ERR("app_serial_pm(resume) failed: %d", ret);
		return ret;
	}

	ret = app_serial_wake(WAKE_LOW_MS, WAKE_HIGH_MS);
	if (ret) {
		LOG_ERR("app_serial_wake failed: %d", ret);
		return ret;
	}

	app_serial_flush_rx();

	ret = app_serial_send(tx, tx_len);
	if (ret < 0) {
		LOG_ERR("app_serial_send failed: %d", ret);
		return ret;
	}

	size_t rx_len = 0;
	ret = app_serial_receive_timeout(rx, expect_len, &rx_len, RESPONSE_TIMEOUT_MS);
	if (ret && rx_len == 0) {
		LOG_WRN("No response from probe (is the ON/OFF switch on?)");
		return -ETIMEDOUT;
	}

	if (rx_len != expect_len) {
		LOG_WRN("Short reply: got %zu of %zu bytes", rx_len, expect_len);
		return -EBADMSG;
	}

	/* Start byte, nB field and checksum are all validated by the pure codec. */
	ret = drv_piketronic_rppu_verify_frame(rx, expect_len);
	if (ret) {
		LOG_WRN("Malformed reply frame: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Park RX_in low so the probe can power down between reads.
 *
 * The vendor rates the internal accumulator for ~1 year only if RX_in is held at 0 while
 * idle; a UART TX pin left idling high keeps the probe awake permanently.
 */
static void park_line(void)
{
	int ret = app_serial_set_break(true);

	if (ret) {
		LOG_WRN("Could not park line low: %d (probe will not sleep)", ret);
	}
}

/**
 * @brief Park the probe's RX_in low, then power the UART bridge down.
 *
 * Call after every transaction. Leaving the SC16IS740 resumed costs ~6 mA continuously,
 * which dominates the whole energy budget on this application.
 */
static void idle_down(void)
{
	park_line();

	int ret = app_serial_pm(false);

	if (ret) {
		LOG_WRN("app_serial_pm(suspend) failed: %d (idle current will be high)", ret);
	}
}

/* Deliberately NOT touching the RS-485 DE/RE pins.
 *
 * The RPP-U talks over the TTL pins (X2 pin 3/4), so the RS-485 transceiver is unused
 * here, and in transparent mode Zephyr's modbus-serial driver never initialises - which
 * makes it tempting to drive DE/RE inactive to save the transceiver's receive-mode
 * quiescent current.
 *
 * That was tried and measured to be counter-productive: driving them inactive at init
 * took the board from 0.44 mA to 0.70 mA (S/N 2159019036, battery + multimeter). The X2
 * shield holds these pins at a sane default through pull resistors, so driving them as
 * outputs only fights those pulls - holding RE high against a ~10k pull-down costs
 * ~330 uA, matching the observed increase.
 *
 * Leave the pins alone. The modbus driver owns them when RS-485 is actually in use.
 */

static int init(struct app_driver_slot *slot)
{
	struct rppu_ctx *ctx = slot->ctx;

	LOG_INF("Initializing Piketronic RPP-U driver (115200 8N1 expected)");

	if (g_app_config.serial_baudrate != 115200) {
		LOG_WRN("serial-baudrate is %d, RPP-U needs 115200",
			g_app_config.serial_baudrate);
	}

	if (g_app_config.serial_parity != APP_CONFIG_SERIAL_PARITY_NONE) {
		LOG_WRN("serial-parity is not 'none', RPP-U needs 8N1");
	}

	if (g_app_config.serial_mode != APP_CONFIG_SERIAL_MODE_UART) {
		LOG_WRN("serial-mode is not 'uart' - RPP-U is a point-to-point UART device");
	}

	memset(&ctx->data, 0, sizeof(ctx->data));
	ctx->alert_armed = true;
	ctx->alert_last_uptime = -1;

	idle_down();

	return 0;
}

/* Trigger an immediate uplink when the accumulator crosses the configured threshold.
 *
 * @p volt of 0 means the probe did not report a voltage, which is not the same as a flat
 * accumulator, so it is ignored rather than treated as the worst case.
 */
static void check_accumulator_alert(struct rppu_ctx *ctx, uint16_t volt)
{
	int threshold = g_app_config.rppu_voltage_threshold_mv;

	if (threshold <= 0 || volt == 0) {
		return;
	}

	if (volt > threshold + ALERT_HYSTERESIS_MV) {
		if (!ctx->alert_armed) {
			LOG_INF("RPP-U accumulator recovered to %u mV, alert re-armed", volt);
		}
		ctx->alert_armed = true;
		return;
	}

	if (volt > threshold || !ctx->alert_armed) {
		return;
	}

	int64_t now = k_uptime_get() / MSEC_PER_SEC;

	if (ctx->alert_last_uptime >= 0 && (now - ctx->alert_last_uptime) < ALERT_MIN_INTERVAL_S) {
		LOG_INF("RPP-U accumulator at %u mV (<= %d mV) - alert suppressed, last one %lld s ago",
			volt, threshold, now - ctx->alert_last_uptime);
		return;
	}

	ctx->alert_armed = false;
	ctx->alert_last_uptime = now;

	LOG_WRN("RPP-U accumulator %u mV at or below threshold %d mV - reporting immediately",
		volt, threshold);

	app_work_send();
}

static int sample(struct app_driver_slot *slot)
{
	struct rppu_ctx *ctx = slot->ctx;
	struct drv_piketronic_rppu_data *data = &ctx->data;
	uint8_t rx[DATA_FRAME_LEN];
	int ret;

	LOG_INF("Sampling Piketronic RPP-U");

	ret = transact(CMD_DATA, sizeof(CMD_DATA), rx, DATA_FRAME_LEN);
	if (ret) {
		data->read_fail++;
		/* Invalidate the snapshot, matching the RPP-R (drv_piketronic_rpp.c). The LRW
		 * encoder sends a live snapshot rather than buffered samples, so leaving
		 * valid=true here made it repeat the last good reading indefinitely - and a
		 * probe that has powered itself off on a flat accumulator is exactly when that
		 * happens. It would report a healthy frozen voltage instead of a gap, hiding
		 * the event the voltage field exists to expose. CBOR is unaffected: it sends
		 * buffered samples and simply emits none.
		 */
		data->valid = false;
		idle_down();
		return ret;
	}

	ret = drv_piketronic_rppu_decode_data(rx, DATA_FRAME_LEN, data);
	if (ret) {
		/* transact() already verified the frame, so this should not happen. */
		data->read_fail++;
		data->valid = false;
		LOG_ERR("Failed to decode DATA frame: %d", ret);
		idle_down();
		return ret;
	}

	data->valid = true;
	data->read_ok++;

	/* Record a sample for the next uplink: the four RPP-R radon fields plus the
	 * accumulator voltage. Voltage is not shared with the RPP-R (which is bus-powered) -
	 * this probe runs off an accumulator and powers itself off below 3300 mV, so without
	 * it in telemetry an operator cannot see a probe about to go dark remotely.
	 */
	int64_t ts;
	ctr_rtc_get_ts(&ts);

	struct piketronic_rppu_sample rec = {
		.timestamp = ts,
		.concentration = data->concentration_ac,
		.concentration_day = data->concentration_day_ac,
		.temperature = data->temperature,
		.humidity = data->humidity,
		.voltage_mv = data->voltage_mv,
	};

	app_driver_rec_push(slot, &rec);

	LOG_INF("RPP-U: RnA=%u RnAC=%u Bq/m3, T=%d C, RH=%u %%, %u mV, switch=%u, err=0x%02x",
		data->concentration_a, data->concentration_ac, data->temperature, data->humidity,
		data->voltage_mv, data->switch_state, data->errors);

	if (data->errors != 0) {
		LOG_WRN("RPP-U reports error flags 0x%02x", data->errors);
	}

	if (data->voltage_mv != 0 && data->voltage_mv < 3400) {
		LOG_WRN("RPP-U accumulator low (%u mV, powers off below 3300)", data->voltage_mv);
	}

	check_accumulator_alert(ctx, data->voltage_mv);

	if (data->switch_state == 0) {
		LOG_WRN("RPP-U measurement switch is OFF - values will not update");
	}

	idle_down();

	return 0;
}

const struct drv_piketronic_rppu_data *piketronic_rppu_get_data(const struct app_driver_slot *slot)
{
	const struct rppu_ctx *ctx = slot->ctx;

	return &ctx->data;
}

int drv_piketronic_rppu_read_serial(char *buf, size_t size)
{
	uint8_t rx[SERIAL_FRAME_LEN];
	int ret;

	if (buf == NULL || size < SERIAL_ASCII_LEN + 1) {
		return -EINVAL;
	}

	ret = transact(CMD_SERIAL, sizeof(CMD_SERIAL), rx, SERIAL_FRAME_LEN);
	idle_down();
	if (ret) {
		return ret;
	}

	memcpy(buf, &rx[4], SERIAL_ASCII_LEN);
	buf[SERIAL_ASCII_LEN] = '\0';

	/* The field is NUL-padded; trim trailing NULs and spaces */
	for (int i = SERIAL_ASCII_LEN - 1; i >= 0; i--) {
		if (buf[i] == '\0' || buf[i] == ' ') {
			buf[i] = '\0';
		} else {
			break;
		}
	}

	return 0;
}

static void print_data(const struct shell *shell, struct app_driver_slot *slot)
{
	const struct drv_piketronic_rppu_data *d = piketronic_rppu_get_data(slot);

	if (!d->valid) {
		shell_print(shell, "[%d] Piketronic RPP-U: no data yet", slot->idx);
		return;
	}

	shell_print(shell, "[%d] Piketronic RPP-U (UART):", slot->idx);
	shell_print(shell, "  radon A (0.5h):   %u Bq/m3", d->concentration_a);
	shell_print(shell, "  radon AC (0.5h):  %u Bq/m3", d->concentration_ac);
	shell_print(shell, "  radon AC (1d):    %u Bq/m3", d->concentration_day_ac);
	shell_print(shell, "  temperature:      %d C", d->temperature);
	shell_print(shell, "  humidity:         %u %%", d->humidity);
	shell_print(shell, "  cycle timer:      %u s (into 120 s)", d->concentration_time);
	shell_print(shell, "  accumulator:      %u mV", d->voltage_mv);
	shell_print(shell, "  switch:           %u", d->switch_state);
	shell_print(shell, "  errors:           0x%02x", d->errors);
	shell_print(shell, "  records:          %u", d->record_count);
	shell_print(shell, "  impulses total:   %u", d->impulses_total);
	shell_print(shell, "  probe RTC:        %u s since 2000", d->real_time);
	shell_print(shell, "  reads ok/fail:    %u/%u", d->read_ok, d->read_fail);
}

const struct app_device_driver piketronic_rppu_driver = {
	.name = "piketronic_u",
	.type = APP_DEVICE_TYPE_PIKETRONIC_RPPU,
	.transport = APP_DEVICE_TRANSPORT_UART,
	.max_instances = 1, /* UART is point-to-point and the frame has no address */
	.ctx_size = sizeof(struct rppu_ctx),
	.rec_size = sizeof(struct piketronic_rppu_sample),
	.init = init,
	.sample = sample,
	.print_data = print_data,
};

/* Shell commands */

static int find_rppu_device(void)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type == APP_DEVICE_TYPE_PIKETRONIC_RPPU) {
			return i;
		}
	}
	return -1;
}

static int cmd_rppu_sample(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int device_idx = find_rppu_device();
	if (device_idx < 0) {
		shell_error(shell, "Piketronic RPP-U device not configured");
		return -ENODEV;
	}

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

static int cmd_rppu_serial(const struct shell *shell, size_t argc, char **argv)
{
	char sn[SERIAL_ASCII_LEN + 1];

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Hold the slot of a configured probe, so a scheduled sample cannot interleave with
	 * this transaction
	 */
	int device_idx = find_rppu_device();
	struct app_driver_slot *slot = device_idx < 0 ? NULL : app_device_get_slot(device_idx);

	if (slot) {
		app_driver_slot_lock(slot);
	}

	int ret = drv_piketronic_rppu_read_serial(sn, sizeof(sn));

	if (slot) {
		app_driver_slot_unlock(slot);
	}

	if (ret) {
		shell_error(shell, "Serial number read failed: %d", ret);
		return ret;
	}

	shell_print(shell, "serial number: %s", sn);

	return 0;
}

/* clang-format off */

const struct shell_static_entry piketronic_rppu_shell_subcmds[] = {
	SHELL_CMD_ARG(sample, NULL, "Read actual data (D)", cmd_rppu_sample, 1, 0),
	SHELL_CMD_ARG(serial, NULL, "Read serial number (V)", cmd_rppu_serial, 1, 0),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry piketronic_rppu_shell_cmds = {
	.entry = piketronic_rppu_shell_subcmds
};

/* clang-format on */
