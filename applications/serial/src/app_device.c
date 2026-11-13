/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file app_device.c
 * @brief Device manager implementation
 *
 * This module provides a unified interface for all device drivers.
 * It handles driver registration, device lookup, and API dispatch.
 */

#include "app_device.h"
#include "app_config.h"
#include "app_data.h"
#include "app_driver_store.h"
#include "app_modbus.h"
#include "drivers/drv_interface.h"

#include <chester/ctr_rtc.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_device, LOG_LEVEL_INF);

/* Shared storage pool of the slot-aware drivers (see app_driver_store.h); it grows as the
 * drivers move from their static single-instance buffers: energy meters 10752 B, FlowT,
 * Piketronic and SolaX 4352 B, MicroSENS and Promag 1280 B, Piketronic RPP-U 768 B */
#define POOL_SIZE 17152

/* Storage of the probe slot (shell commands addressing a device directly): the driver state
 * or the item states, without rings; probe_setup() checks the actual demand */
#define PROBE_SIZE 512

BUILD_ASSERT(PROBE_SIZE >= ROUND_UP(sizeof(struct app_driver_item_state) * APP_DRIVER_MAX_ITEMS, 8),
	     "The probe slot does not hold APP_DRIVER_MAX_ITEMS item states");

/* Pause before a register read that timed out is sent again. A reply that was already on its
 * way is received and dropped by then, also at 1200 Bd: up to 9 bytes, the RX batching of the
 * SC16IS7xx bridge and the 50 ms RTU frame timeout (app_modbus_init()) */
#define RETRY_DELAY_MS 200

static struct app_driver_slot m_slots[APP_CONFIG_MAX_DEVICES];
static uint8_t m_pool[POOL_SIZE] __aligned(8);

static struct app_driver_slot m_probe_slot;
static uint8_t m_probe_mem[PROBE_SIZE] __aligned(8);
static K_MUTEX_DEFINE(m_probe_mutex);
static bool m_probe_lock_ready;

/* Device type names for string conversion */
static const char *const m_type_names[] = {
	[APP_DEVICE_TYPE_NONE] = "none",
	[APP_DEVICE_TYPE_MICROSENS_180HS] = "microsens_180hs",
	[APP_DEVICE_TYPE_SENSECAP_S1000] = "sensecap_s1000",
	[APP_DEVICE_TYPE_CUBIC_6303] = "cubic_6303",
	[APP_DEVICE_TYPE_LAMBRECHT] = "lambrecht",
	[APP_DEVICE_TYPE_GENERIC] = "generic",
	/* Energy meters */
	[APP_DEVICE_TYPE_OR_WE_504] = "or_we_504",
	[APP_DEVICE_TYPE_EM1XX] = "em1xx",
	[APP_DEVICE_TYPE_OR_WE_516] = "or_we_516",
	[APP_DEVICE_TYPE_EM5XX] = "em5xx",
	[APP_DEVICE_TYPE_IEM3000] = "iem3000",
	[APP_DEVICE_TYPE_PROMAG_MF7S] = "promag_mf7s",
	[APP_DEVICE_TYPE_FLOWT_FT201] = "flowt_ft201",
	[APP_DEVICE_TYPE_PIKETRONIC_RPP] = "piketronic",
	[APP_DEVICE_TYPE_SOLAX_G3] = "solax_g3",
	[APP_DEVICE_TYPE_PIKETRONIC_RPPU] = "piketronic_u",
	[APP_DEVICE_TYPE_SENSECAP_S500] = "sensecap_s500",
	[APP_DEVICE_TYPE_SENSECAP_S700] = "sensecap_s700",
};

const struct app_device_driver *app_device_find_driver(enum app_device_type type)
{
	switch (type) {
	case APP_DEVICE_TYPE_MICROSENS_180HS:
		return &microsens_driver;
	case APP_DEVICE_TYPE_OR_WE_504:
		return &or_we_504_driver;
	case APP_DEVICE_TYPE_EM1XX:
		return &em1xx_driver;
	case APP_DEVICE_TYPE_OR_WE_516:
		return &or_we_516_driver;
	case APP_DEVICE_TYPE_EM5XX:
		return &em5xx_driver;
	case APP_DEVICE_TYPE_IEM3000:
		return &iem3000_driver;
	case APP_DEVICE_TYPE_PROMAG_MF7S:
		return &promag_mf7s_driver;
	case APP_DEVICE_TYPE_FLOWT_FT201:
		return &flowt_ft201_driver;
	case APP_DEVICE_TYPE_PIKETRONIC_RPP:
		return &piketronic_rpp_driver;
	case APP_DEVICE_TYPE_SOLAX_G3:
		return &solax_g3_driver;
	case APP_DEVICE_TYPE_PIKETRONIC_RPPU:
		return &piketronic_rppu_driver;
	case APP_DEVICE_TYPE_SENSECAP_S1000:
		return &sensecap_s1000_driver;
	case APP_DEVICE_TYPE_SENSECAP_S500:
		return &sensecap_s500_driver;
	case APP_DEVICE_TYPE_SENSECAP_S700:
		return &sensecap_s700_driver;
	case APP_DEVICE_TYPE_CUBIC_6303:
		return &cubic_6303_driver;
	default:
		return NULL;
	}
}

static void get_demand(const struct app_device_driver *drv, struct app_driver_demand *demand)
{
	*demand = (struct app_driver_demand){
		.ctx_size = drv->ctx_size,
		.rec_size = drv->rec_size,
		.rec_fixed = drv->rec_fixed,
		.items = drv->items,
		.item_count = drv->item_count,
	};
}

static void get_params(int interval_sample, int interval_aggreg, int interval_report, int mode,
		       struct app_driver_plan_params *params)
{
	*params = (struct app_driver_plan_params){
		.interval_sample = interval_sample,
		.interval_aggreg = interval_aggreg,
		.interval_report = interval_report,
		/* Only LTE reports carry the history; LoRaWAN sends the latest data */
		.keep_history = mode == APP_CONFIG_MODE_LTE,
	};
}

/* The rings are capped (app_driver_store.h): tell where one report period does not fit */
static void warn_short_buffers(const struct shell *shell,
			       const struct app_driver_demand *const *demands,
			       const struct app_driver_plan_params *params,
			       const struct app_driver_slot_plan *plans)
{
	if (!params->keep_history || params->interval_report <= 0) {
		return;
	}

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		const struct app_driver_demand *demand = demands[i];
		uint32_t need;

		if (!demand) {
			continue;
		}

		if (demand->rec_size && !demand->rec_fixed && params->interval_sample > 0) {
			need = DIV_ROUND_UP(params->interval_report, params->interval_sample) + 1;

			if (plans[i].rec_cap < need) {
				if (shell) {
					shell_warn(shell,
						   "device-%d: keeps %u of the %u samples of a "
						   "report period, older ones are dropped",
						   i, plans[i].rec_cap, need);
				} else {
					LOG_WRN("Device[%d]: keeps %u of the %u samples of a "
						"report "
						"period",
						i, plans[i].rec_cap, need);
				}
			}
		}

		if (demand->item_count && params->interval_aggreg > 0) {
			need = DIV_ROUND_UP(params->interval_report, params->interval_aggreg) + 1;

			if (plans[i].win_cap < need) {
				if (shell) {
					shell_warn(shell,
						   "device-%d: keeps %u of the %u windows of a "
						   "report period, older ones are dropped",
						   i, plans[i].win_cap, need);
				} else {
					LOG_WRN("Device[%d]: keeps %u of the %u windows of a "
						"report "
						"period",
						i, plans[i].win_cap, need);
				}
			}
		}
	}
}

int app_device_check_config(const struct shell *shell, const struct app_config_device *devices,
			    int interval_sample, int interval_aggreg, int interval_report, int mode)
{
	struct app_driver_demand demand[APP_CONFIG_MAX_DEVICES];
	const struct app_driver_demand *demands[APP_CONFIG_MAX_DEVICES] = {0};
	struct app_driver_slot_plan plans[APP_CONFIG_MAX_DEVICES];
	struct app_driver_plan_params params;
	int count[APP_DEVICE_TYPE_COUNT] = {0};

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = devices[i].type;

		if (type <= APP_DEVICE_TYPE_NONE || type >= APP_DEVICE_TYPE_COUNT) {
			continue;
		}

		const struct app_device_driver *drv = app_device_find_driver(type);

		if (!drv) {
			continue;
		}

		if (drv->max_instances && ++count[type] > drv->max_instances) {
			shell_error(shell, "device-%d: at most %u %s device(s) can be configured",
				    i, drv->max_instances, app_device_type_name(type));
			return -EINVAL;
		}

		get_demand(drv, &demand[i]);
		demands[i] = &demand[i];
	}

	get_params(interval_sample, interval_aggreg, interval_report, mode, &params);

	if (app_driver_pool_plan(demands, APP_CONFIG_MAX_DEVICES, &params, sizeof(m_pool), plans)) {
		shell_error(shell, "Device data for one report period does not fit the memory");
		shell_error(shell, "Shorten interval-report, lengthen interval-sample / "
				   "interval-aggreg or configure fewer devices");
		return -ENOSPC;
	}

	warn_short_buffers(shell, demands, &params, plans);

	return 0;
}

/* Transport of a configured device: the one of its driver. The driverless legacy types are
 * served by the switch inside app_modbus.c, only transparent mode keeps them on the raw UART.
 */
static enum app_device_transport device_transport(const struct app_config_device *dev,
						  int serial_mode)
{
	if (app_device_has_driver(dev->type)) {
		return app_device_get_transport(dev->type);
	}

	return serial_mode == APP_CONFIG_SERIAL_MODE_TRANSPARENT ? APP_DEVICE_TRANSPORT_UART
								 : APP_DEVICE_TRANSPORT_MODBUS;
}

static const char *transport_name(enum app_device_transport transport)
{
	return transport == APP_DEVICE_TRANSPORT_UART ? "UART" : "Modbus";
}

bool app_device_pick_bus(const struct app_config_device *devices, int serial_mode,
			 enum app_device_transport *bus)
{
	bool uart = false;
	bool modbus = false;

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = devices[i].type;

		if (type <= APP_DEVICE_TYPE_NONE || type >= APP_DEVICE_TYPE_COUNT) {
			continue;
		}

		if (device_transport(&devices[i], serial_mode) == APP_DEVICE_TRANSPORT_UART) {
			uart = true;
		} else {
			modbus = true;
		}
	}

	if (uart && modbus) {
		*bus = serial_mode == APP_CONFIG_SERIAL_MODE_MODBUS ? APP_DEVICE_TRANSPORT_MODBUS
								    : APP_DEVICE_TRANSPORT_UART;
	} else {
		*bus = uart ? APP_DEVICE_TRANSPORT_UART : APP_DEVICE_TRANSPORT_MODBUS;
	}

	return uart || modbus;
}

static int plan_slots(void)
{
	struct app_driver_demand demand[APP_CONFIG_MAX_DEVICES];
	const struct app_driver_demand *demands[APP_CONFIG_MAX_DEVICES] = {0};
	struct app_driver_slot_plan plans[APP_CONFIG_MAX_DEVICES];
	struct app_driver_plan_params params;
	int count[APP_DEVICE_TYPE_COUNT] = {0};
	enum app_device_transport bus;
	int uart_device = -1;
	int ret;

	app_device_pick_bus(g_app_config.devices, g_app_config.serial_mode, &bus);

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = &m_slots[i];
		enum app_device_type type = g_app_config.devices[i].type;

		k_mutex_init(&slot->lock);
		slot->idx = i;
		slot->addr = g_app_config.devices[i].addr;
		slot->drv = NULL;
		slot->initialized = false;

		if (type <= APP_DEVICE_TYPE_NONE || type >= APP_DEVICE_TYPE_COUNT) {
			continue;
		}

		/* The bus runs one transport, and UART is point-to-point */
		enum app_device_transport transport =
			device_transport(&g_app_config.devices[i], g_app_config.serial_mode);

		if (transport != bus) {
			LOG_ERR("Device[%d] needs %s, but the bus runs %s: not active", i,
				transport_name(transport), transport_name(bus));
			continue;
		}

		if (bus == APP_DEVICE_TRANSPORT_UART) {
			if (uart_device >= 0) {
				LOG_ERR("Device[%d]: UART is point-to-point and serves device[%d]: "
					"not active",
					i, uart_device);
				continue;
			}

			uart_device = i;
		}

		const struct app_device_driver *drv = app_device_find_driver(type);

		if (!drv) {
			LOG_WRN("No driver for device[%d] type %d", i, type);
			continue;
		}

		/* Reports encode either the item windows or the records of a device */
		if (drv->items && drv->rec_size) {
			LOG_ERR("Device[%d] '%s': driver has both items and records, ignored", i,
				drv->name);
			continue;
		}

		if (drv->max_instances && ++count[type] > drv->max_instances) {
			LOG_ERR("Device[%d]: at most %u %s device(s) supported, ignored", i,
				drv->max_instances, drv->name);
			continue;
		}

		slot->drv = drv;
		get_demand(drv, &demand[i]);
		demands[i] = &demand[i];
	}

	get_params(g_app_config.interval_sample, g_app_config.interval_aggreg,
		   g_app_config.interval_report, g_app_config.mode, &params);

	for (;;) {
		ret = app_driver_pool_plan(demands, APP_CONFIG_MAX_DEVICES, &params, sizeof(m_pool),
					   plans);
		if (ret != -ENOMEM) {
			break;
		}

		/* Not even the minimum fits: give up on the last slot-aware device */
		int last = -1;

		for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
			if (demands[i]) {
				last = i;
			}
		}

		if (last < 0) {
			return -ENOMEM;
		}

		LOG_ERR("Device[%d]: no memory left, ignored", last);
		demands[last] = NULL;
		m_slots[last].drv = NULL;
	}

	if (ret == -ENOSPC) {
		LOG_WRN("Device buffers reduced to fit the memory: they hold less than one "
			"report period");
	}

	warn_short_buffers(NULL, demands, &params, plans);

	uint8_t *mem = m_pool;

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (!demands[i]) {
			continue;
		}

		app_driver_slot_setup(&m_slots[i], demands[i], &plans[i], mem,
				      g_app_config.interval_aggreg);
		mem += plans[i].bytes;

		LOG_INF("Device[%d] '%s': %u records, %u windows, %zu B", i, m_slots[i].drv->name,
			plans[i].rec_cap, plans[i].win_cap, plans[i].bytes);
	}

	return 0;
}

int app_device_init(void)
{
	int ret;
	int init_count = 0;
	int error_count = 0;

	LOG_INF("Initializing device drivers");

	ret = plan_slots();
	if (ret) {
		LOG_ERR("Call `plan_slots` failed: %d", ret);
		return ret;
	}

	/* Initialize drivers for all configured devices */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = &m_slots[i];
		const struct app_device_driver *drv = slot->drv;

		if (!drv || !drv->init) {
			continue;
		}

		LOG_INF("Initializing device[%d] driver '%s'", i, drv->name);

		app_driver_slot_lock(slot);
		ret = drv->init(slot);
		slot->initialized = !ret;
		app_driver_slot_unlock(slot);

		if (ret) {
			/* Retried before the next sample */
			LOG_ERR("Failed to init '%s': %d", drv->name, ret);
			error_count++;
		} else {
			LOG_INF("Initialized '%s' driver", drv->name);
			init_count++;
		}
	}

	if (init_count == 0 && error_count == 0) {
		LOG_INF("No devices configured");
		return 0;
	}

	LOG_INF("Device init complete: %d initialized, %d errors", init_count, error_count);

	return error_count > 0 ? -EIO : 0;
}

struct app_driver_slot *app_device_get_slot(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES || !m_slots[device_idx].drv) {
		return NULL;
	}

	return &m_slots[device_idx];
}

/* Register value scaled by its divisor; float registers and a zero divisor are not divided */
static float apply_div(enum app_modbus_format fmt, float div, float value)
{
	if (fmt == APP_MODBUS_FLOAT_HI || fmt == APP_MODBUS_FLOAT_LO || div == 0.f) {
		return value;
	}

	return value / div;
}

int app_device_sample_items(struct app_driver_slot *slot)
{
	const struct app_device_driver *drv = slot->drv;
	int reads = 0;
	int errors = 0;
	int ret;

	ret = app_modbus_acquire();
	if (ret) {
		return ret;
	}

	for (size_t i = 0; i < drv->item_count; i++) {
		const struct app_driver_item *item = &drv->items[i];
		float value;

		if (item->reg == APP_DRIVER_REG_NONE) {
			continue;
		}

		reads++;

		ret = app_modbus_read_value(slot->addr, item->reg, drv->fc, item->fmt, &value);
		if (ret) {
			errors++;
			continue;
		}

		app_driver_items_put(slot, i, apply_div(item->fmt, item->div, value));
	}

	app_modbus_release();

	if (errors) {
		LOG_WRN("Device[%d] '%s': %d of %d reads failed", slot->idx, drv->name, errors,
			reads);
	}

	return reads && errors == reads ? -EIO : 0;
}

/* Caller holds slot->lock */
static int sample_slot(struct app_driver_slot *slot)
{
	const struct app_device_driver *drv = slot->drv;
	int ret;

	if (drv->init && !slot->initialized) {
		ret = drv->init(slot);
		if (ret) {
			LOG_WRN("Device[%d] '%s' init failed: %d", slot->idx, drv->name, ret);
			return ret;
		}

		slot->initialized = true;
	}

	if (drv->sample) {
		return drv->sample(slot);
	}

	if (drv->items) {
		return app_device_sample_items(slot);
	}

	return -ENOTSUP;
}

int app_device_read_regs(uint8_t addr, enum app_modbus_fc fc, const struct app_device_reg *regs,
			 size_t count, void *dst, bool stop_on_error)
{
	int ret;
	int errors = 0;
	int first_error = 0;

	for (size_t i = 0; i < count; i++) {
		const struct app_device_reg *r = &regs[i];
		float value;

		ret = app_modbus_read_value(addr, r->reg, fc, r->fmt, &value);

		/* The device answered every read before this one, so it missed only this request:
		 * ask once more. Not the first read, so a device that does not answer is not
		 * waited for twice, and not after a failed read, as the call fails anyway */
		if (ret == -ETIMEDOUT && i > 0 && !errors) {
			LOG_WRN("No reply for %s (0x%04x) at addr %u, retrying", r->name, r->reg,
				addr);
			k_sleep(K_MSEC(RETRY_DELAY_MS));
			ret = app_modbus_read_value(addr, r->reg, fc, r->fmt, &value);
		}

		if (ret) {
			LOG_ERR("Failed to read %s (0x%04x) at addr %u: %d", r->name, r->reg, addr,
				ret);

			if (stop_on_error) {
				return ret;
			}

			/* The device did not answer the first read or the repeat: the other reads
			 * would only wait for the timeout too, and the call fails anyway */
			if (ret == -ETIMEDOUT && !errors) {
				LOG_ERR("No reply at addr %u, stopped after %zu of %zu reads", addr,
					i + 1, count);
				return ret;
			}

			errors++;
			first_error = first_error ? first_error : ret;
			continue;
		}

		value = apply_div(r->fmt, r->div, value);

		memcpy((uint8_t *)dst + r->offset, &value, sizeof(value));
	}

	if (errors) {
		LOG_ERR("%d of %zu reads failed at addr %u", errors, count, addr);
		return first_error;
	}

	return 0;
}

void app_device_print_header(const struct shell *shell, const struct app_driver_slot *slot,
			     const char *title)
{
	if (slot->idx < 0) {
		shell_print(shell, "%s @ addr %u:", title, slot->addr);
	} else {
		shell_print(shell, "[%d] %s @ addr %u:", slot->idx, title, slot->addr);
	}
}

void app_device_print_items(const struct shell *shell, struct app_driver_slot *slot,
			    const char *title)
{
	const struct app_device_driver *drv = slot->drv;
	bool closed = app_driver_win_count(slot) > 0;
	uint32_t seq = app_driver_win_end(slot) - 1;

	app_device_print_header(shell, slot, title);

	for (size_t i = 0; i < drv->item_count; i++) {
		const struct app_driver_item *item = &drv->items[i];
		double last = app_driver_items_last(slot, i);

		if (!closed) {
			shell_print(shell, "  %-22s %.3f %s", item->name, last, item->unit);
		} else if (item->mode == APP_DRIVER_AGGREG_STATS) {
			const struct ctr_data_aggreg *w = app_driver_win_stats(slot, i, seq);

			shell_print(shell,
				    "  %-22s %.3f %s (window min/max/avg/mdn %.3f/%.3f/%.3f/%.3f)",
				    item->name, last, item->unit, (double)w->min, (double)w->max,
				    (double)w->avg, (double)w->mdn);
		} else {
			shell_print(shell, "  %-22s %.3f %s (window %.3f)", item->name, last,
				    item->unit, (double)app_driver_win_value(slot, i, seq));
		}
	}
}

void app_device_aggreg_all(uint32_t ticks)
{
	int64_t ts;
	int64_t uptime = k_uptime_get();

	ctr_rtc_get_ts(&ts);

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = &m_slots[i];

		if (slot->drv && slot->drv->items) {
			app_driver_slot_lock(slot);
			app_driver_items_aggreg(slot, ts, uptime, ticks);
			app_driver_slot_unlock(slot);
		}
	}
}

void app_device_clear_all(void)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		struct app_driver_slot *slot = &m_slots[i];

		if (slot->drv) {
			app_driver_slot_lock(slot);
			app_driver_slot_clear(slot);
			app_driver_slot_unlock(slot);
		}
	}
}

/* Set up the probe slot for a driver at an address; caller holds m_probe_mutex */
static int probe_setup(const struct app_device_driver *drv, uint8_t addr)
{
	struct app_driver_demand demand;
	struct app_driver_slot_plan plan = {0};

	/* The shell can probe before app_device_init(); this lock is only taken under
	 * m_probe_mutex
	 */
	if (!m_probe_lock_ready) {
		k_mutex_init(&m_probe_slot.lock);
		m_probe_lock_ready = true;
	}

	get_demand(drv, &demand);

	/* Latest values only, nothing is kept for upload */
	plan.bytes = app_driver_slot_bytes(&demand, 0, 0, 0);
	if (plan.bytes > sizeof(m_probe_mem)) {
		return -ENOMEM;
	}

	m_probe_slot.idx = -1;
	m_probe_slot.addr = addr;
	m_probe_slot.drv = drv;
	m_probe_slot.initialized = false;

	app_driver_slot_setup(&m_probe_slot, &demand, &plan, m_probe_mem, 0);

	return 0;
}

int app_device_probe_run(const struct shell *shell, const struct app_device_driver *drv,
			 uint8_t addr,
			 int (*op)(const struct shell *shell, struct app_driver_slot *slot))
{
	int ret;

	k_mutex_lock(&m_probe_mutex, K_FOREVER);

	ret = probe_setup(drv, addr);
	if (!ret) {
		app_driver_slot_lock(&m_probe_slot);
		ret = op(shell, &m_probe_slot);
		app_driver_slot_unlock(&m_probe_slot);
	}

	k_mutex_unlock(&m_probe_mutex);

	return ret;
}

static int probe_sample_op(const struct shell *shell, struct app_driver_slot *slot)
{
	int ret = sample_slot(slot);

	if (!ret && slot->drv->print_data) {
		slot->drv->print_data(shell, slot);
	}

	return ret;
}

int app_device_probe_sample(const struct shell *shell, const struct app_device_driver *drv,
			    uint8_t addr)
{
	return app_device_probe_run(shell, drv, addr, probe_sample_op);
}

int app_device_probe_config(const struct shell *shell, const struct app_device_driver *drv,
			    uint8_t addr, uint8_t new_addr, const char *parity_str)
{
	int ret;

	ARG_UNUSED(shell);

	if (!drv->config) {
		return -ENOTSUP;
	}

	k_mutex_lock(&m_probe_mutex, K_FOREVER);

	ret = probe_setup(drv, addr);
	if (!ret) {
		app_driver_slot_lock(&m_probe_slot);
		ret = drv->config(&m_probe_slot, new_addr, parity_str);
		app_driver_slot_unlock(&m_probe_slot);
	}

	k_mutex_unlock(&m_probe_mutex);

	return ret;
}

int app_device_sample(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		LOG_ERR("Invalid device index: %d", device_idx);
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[device_idx].type;
	if (type == APP_DEVICE_TYPE_NONE) {
		LOG_DBG("Device[%d] not configured", device_idx);
		return -ENODEV;
	}

	struct app_driver_slot *slot = &m_slots[device_idx];
	const struct app_device_driver *drv = slot->drv;

	if (drv == NULL) {
		LOG_ERR("No driver for device[%d] type %d", device_idx, type);
		return -ENOTSUP;
	}

	LOG_DBG("Sampling device[%d] (%s) at addr %d", device_idx, drv->name, slot->addr);

	app_driver_slot_lock(slot);
	int ret = sample_slot(slot);
	app_driver_slot_unlock(slot);

	return ret;
}

int app_device_sample_all(void)
{
	int ret;
	int errors = 0;
	int sampled = 0;

	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_CONFIG_DEVICE_TYPE_NONE) {
			ret = app_device_sample(i);
			if (ret) {
				LOG_WRN("Device[%d] sampling failed: %d", i, ret);
				errors++;
			} else {
				sampled++;
			}

			/* Small delay between devices for RS-485 bus settling
			 * This gives the bus time to stabilize after enable/disable cycles
			 * and helps prevent state corruption between consecutive devices.
			 */
			k_sleep(K_MSEC(50));
		}
	}

	if (sampled == 0) {
		LOG_DBG("No devices configured for sampling");
		return -ENODATA;
	}

	return errors > 0 ? -EIO : 0;
}

int app_device_calibrate(int device_idx, enum app_device_calibration cal_type, int value)
{
	int ret;

	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	struct app_driver_slot *slot = &m_slots[device_idx];
	const struct app_device_driver *drv = slot->drv;

	if (drv == NULL) {
		return -ENODEV;
	}

	if (!drv->calibrate) {
		LOG_WRN("Calibration not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Calibrating device[%d] (%s): cal_type=%d, value=%d", device_idx, drv->name,
		cal_type, value);

	app_driver_slot_lock(slot);
	ret = drv->calibrate(slot, cal_type, value);
	app_driver_slot_unlock(slot);

	return ret;
}

int app_device_reset(int device_idx)
{
	int ret;

	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	struct app_driver_slot *slot = &m_slots[device_idx];
	const struct app_device_driver *drv = slot->drv;

	if (drv == NULL) {
		return -ENODEV;
	}

	if (!drv->reset) {
		LOG_WRN("Reset not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Resetting device[%d] (%s)", device_idx, drv->name);

	app_driver_slot_lock(slot);
	ret = drv->reset(slot);
	app_driver_slot_unlock(slot);

	return ret;
}

const char *app_device_type_name(enum app_device_type type)
{
	if (type >= 0 && type < ARRAY_SIZE(m_type_names) && m_type_names[type] != NULL) {
		return m_type_names[type];
	}
	return "unknown";
}

enum app_device_transport app_device_get_transport(enum app_device_type type)
{
	const struct app_device_driver *drv = app_device_find_driver(type);

	/* Types without a driver are served by the legacy switch in app_modbus.c */
	return drv != NULL ? drv->transport : APP_DEVICE_TRANSPORT_MODBUS;
}

bool app_device_has_driver(enum app_device_type type)
{
	return app_device_find_driver(type) != NULL;
}

enum app_device_type app_device_type_from_name(const char *name)
{
	if (name == NULL) {
		return APP_DEVICE_TYPE_NONE;
	}

	for (int i = 0; i < ARRAY_SIZE(m_type_names); i++) {
		if (m_type_names[i] != NULL && strcmp(m_type_names[i], name) == 0) {
			return (enum app_device_type)i;
		}
	}

	/* Also try short names */
	if (strcmp(name, "microsens") == 0) {
		return APP_DEVICE_TYPE_MICROSENS_180HS;
	}
	if (strcmp(name, "sensecap") == 0) {
		return APP_DEVICE_TYPE_SENSECAP_S1000;
	}
	if (strcmp(name, "cubic") == 0) {
		return APP_DEVICE_TYPE_CUBIC_6303;
	}
	/* Energy meter short names */
	if (strcmp(name, "em111") == 0) {
		return APP_DEVICE_TYPE_EM1XX;
	}
	if (strcmp(name, "em540") == 0) {
		return APP_DEVICE_TYPE_EM5XX;
	}
	if (strcmp(name, "promag") == 0) {
		return APP_DEVICE_TYPE_PROMAG_MF7S;
	}
	if (strcmp(name, "flowt") == 0) {
		return APP_DEVICE_TYPE_FLOWT_FT201;
	}
	if (strcmp(name, "solax") == 0) {
		return APP_DEVICE_TYPE_SOLAX_G3;
	}

	return APP_DEVICE_TYPE_NONE;
}

bool app_device_is_configured(int device_idx)
{
	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return false;
	}

	return g_app_config.devices[device_idx].type != APP_CONFIG_DEVICE_TYPE_NONE;
}

int app_device_config(int device_idx, uint8_t new_addr, const char *parity_str)
{
	int ret;

	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		return -EINVAL;
	}

	struct app_driver_slot *slot = &m_slots[device_idx];
	const struct app_device_driver *drv = slot->drv;

	if (drv == NULL) {
		return -ENODEV;
	}

	if (!drv->config) {
		LOG_WRN("Config not supported for device[%d]", device_idx);
		return -ENOTSUP;
	}

	LOG_INF("Configuring device[%d] (%s): new_addr=%d", device_idx, drv->name, new_addr);

	app_driver_slot_lock(slot);
	ret = drv->config(slot, new_addr, parity_str);
	app_driver_slot_unlock(slot);

	return ret;
}

/*
 * Shell commands
 */

/* Device list command */

static int cmd_device_list(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "Configured devices:");

	bool found = false;
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		enum app_device_type type = g_app_config.devices[i].type;
		if (type != APP_DEVICE_TYPE_NONE) {
			const char *type_name = app_device_type_name(type);

			shell_print(shell, "  [%d] %s (addr=%d, timeout=%ds)", i, type_name,
				    g_app_config.devices[i].addr,
				    g_app_config.devices[i].timeout_s);
			found = true;

			struct app_driver_slot *slot = &m_slots[i];

			if (!slot->drv) {
				shell_print(shell, "      not active (see log)");
			} else {
				app_driver_slot_lock(slot);
				shell_print(shell, "      records %u/%u, windows %u/%u, dropped %u",
					    app_driver_rec_count(slot), slot->rec.cap,
					    app_driver_win_count(slot), slot->items.win_cap,
					    slot->drops);
				app_driver_slot_unlock(slot);
			}
		}
	}

	if (!found) {
		shell_print(shell, "  (none)");
	}

	return 0;
}

/* Sample device by index (from config) */
static int cmd_device_sample_idx(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device sample <idx>");
		shell_print(shell, "  idx = 0-%d (from device-0..device-7 config)",
			    APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	int idx = strtol(argv[1], NULL, 0);
	if (idx < 0 || idx >= APP_CONFIG_MAX_DEVICES) {
		shell_error(shell, "Invalid index (0-%d)", APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	enum app_device_type type = g_app_config.devices[idx].type;

	if (type == APP_DEVICE_TYPE_NONE) {
		shell_error(shell, "Device[%d] not configured", idx);
		return -ENODEV;
	}

	int ret = app_device_sample(idx);
	if (ret) {
		shell_error(shell, "Sampling device[%d] failed: %d", idx, ret);
		return ret;
	}

	/* Print results via driver's print_data function */
	uint8_t addr = g_app_config.devices[idx].addr;
	struct app_driver_slot *slot = &m_slots[idx];
	const struct app_device_driver *drv = slot->drv;

	if (drv && drv->print_data) {
		app_driver_slot_lock(slot);
		drv->print_data(shell, slot);
		app_driver_slot_unlock(slot);
	} else {
		const char *type_name = app_device_type_name(type);
		shell_print(shell, "[%d] %s @ addr %d: OK", idx, type_name, addr);
	}

	return 0;
}

/* Reset device by index */
static int cmd_device_reset_idx(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: device reset <idx>");
		return -EINVAL;
	}

	int idx = strtol(argv[1], NULL, 0);
	if (idx < 0 || idx >= APP_CONFIG_MAX_DEVICES) {
		shell_error(shell, "Invalid index (0-%d)", APP_CONFIG_MAX_DEVICES - 1);
		return -EINVAL;
	}

	int ret = app_device_reset(idx);
	if (ret == -ENOTSUP) {
		shell_error(shell, "Reset not supported for device[%d]", idx);
		return ret;
	}
	if (ret == -ENODEV) {
		shell_error(shell, "Device[%d] not configured", idx);
		return ret;
	}
	if (ret) {
		shell_error(shell, "Reset device[%d] failed: %d", idx, ret);
		return ret;
	}

	shell_print(shell, "Device[%d] reset complete", idx);
	return 0;
}

/* clang-format off */

/* Device root command
 *
 * Each device driver exports its shell subcommands (e.g., microsens_shell_cmds from microsens_180hs.h).
 * When adding a new device driver:
 * 1. Create shell commands in the driver file (e.g., sensecap.c)
 * 2. Export subcommand set in driver header (e.g., sensecap_shell_cmds)
 * 3. Add SHELL_CMD entry here referencing the exported subcommands
 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_device,
	/* Device-specific commands (with address parameter) */
	SHELL_CMD(microsens_180hs, &microsens_shell_cmds, "MicroSENS 180-HS CO2 sensor commands", NULL),
	SHELL_CMD(or_we_504, &or_we_504_shell_cmds, "Orno OR-WE-504 single-phase meter", NULL),
	SHELL_CMD(em1xx, &em1xx_shell_cmds, "Carlo Gavazzi EM1XX (EM111) single-phase meter", NULL),
	SHELL_CMD(or_we_516, &or_we_516_shell_cmds, "Orno OR-WE-516 3-phase meter", NULL),
	SHELL_CMD(em5xx, &em5xx_shell_cmds, "Carlo Gavazzi EM5XX (EM540) 3-phase meter", NULL),
	SHELL_CMD(iem3000, &iem3000_shell_cmds, "Schneider iEM3000 3-phase meter", NULL),
	SHELL_CMD(promag_mf7s, &promag_mf7s_shell_cmds, "Promag MF7S RFID reader", NULL),
	SHELL_CMD(flowt_ft201, &flowt_ft201_shell_cmds, "FlowT FT201 ultrasonic flowmeter", NULL),
	SHELL_CMD(piketronic, &piketronic_rpp_shell_cmds, "Piketronic RPP-R radon probe", NULL),
	SHELL_CMD(solax_g3, &solax_g3_shell_cmds, "SolaX X3-Hybrid G3 inverter", NULL),
	SHELL_CMD(piketronic_u, &piketronic_rppu_shell_cmds, "Piketronic RPP-U radon probe (UART)", NULL),
	SHELL_CMD(sensecap, &sensecap_shell_cmds, "SenseCAP S1000 / S700 / S500 weather station",
		  NULL),
	SHELL_CMD(cubic_6303, &cubic_6303_shell_cmds, "Cubic 6303 particulate matter sensor", NULL),
	/* Generic commands (use device index from config) */
	SHELL_CMD_ARG(sample, NULL, "Sample device by index: sample <0-7>", cmd_device_sample_idx, 2, 0),
	SHELL_CMD_ARG(reset, NULL, "Reset device by index: reset <0-7>", cmd_device_reset_idx, 2, 0),
	SHELL_CMD_ARG(list, NULL, "List configured devices", cmd_device_list, 1, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(device, &sub_device, "Device commands", NULL);

/* clang-format on */
