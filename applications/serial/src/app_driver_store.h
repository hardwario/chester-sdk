/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DRIVER_STORE_H_
#define APP_DRIVER_STORE_H_

/**
 * @file app_driver_store.h
 * @brief Per-slot storage of device drivers
 *
 * Every configured device slot owns its driver state, a ring of raw sample records
 * (drivers that report every sample) and an item store (drivers described by an item
 * table, aggregated per interval-aggreg window). The memory is carved from one shared
 * pool according to the configured slots and intervals, see app_driver_pool_plan().
 *
 * Records and windows are addressed by monotonic sequence numbers that are never reset,
 * so a sender can encode a range and later drop exactly that range with *_pop_until(),
 * even if new data arrived (or the oldest data was dropped) in between.
 *
 * Unless stated otherwise, the functions expect the caller to hold slot->lock.
 *
 * This module does not depend on the application configuration or on generated
 * headers, so it is unit-tested on its own.
 */

#include "app_modbus_decode.h"

/* CHESTER includes */
#include <chester/application/ctr_data.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_device_driver;

/** Max items per driver (SenseCAP S1000 has 10); the probe slot in app_device.c is sized for it */
#define APP_DRIVER_MAX_ITEMS 12

/** Max samples kept per item and window for the median (STATS items) */
#define APP_DRIVER_MAX_WINDOW_SAMPLES 32

/** Capacity limits of the record and window rings */
#define APP_DRIVER_RING_MIN      2
#define APP_DRIVER_REC_BASE_MAX  64
#define APP_DRIVER_WIN_BASE_MAX  32
#define APP_DRIVER_RING_GROW_MAX 255

/** Register of an item that is not read but computed by the driver */
#define APP_DRIVER_REG_NONE (-1)

/** What a closed window produces for an item */
enum app_driver_aggreg {
	/** Last sample of the window (status, version) */
	APP_DRIVER_AGGREG_LAST = 0,
	/** Minimum, maximum, average and median (instantaneous quantities) */
	APP_DRIVER_AGGREG_STATS,
	/** Last absolute value of a cumulative counter (rainfall, energy) */
	APP_DRIVER_AGGREG_COUNTER,
	/** Vector mean of an angle in degrees (wind direction) */
	APP_DRIVER_AGGREG_CIRCULAR,
};

/** LoRaWAN encoding of an item */
enum app_driver_lrw {
	/** COUNTER: U32 of value * mul, others: F16 of value */
	APP_DRIVER_LRW_AUTO = 0,
	APP_DRIVER_LRW_F16,
	APP_DRIVER_LRW_U16,
	APP_DRIVER_LRW_U32,
};

/** Description of one measured item of a table-driven driver */
struct app_driver_item {
	const char *name; /**< Shell label, e.g. "air_temperature" */
	const char *unit; /**< Shell label, e.g. "°C" */
	int32_t reg;      /**< Register, or APP_DRIVER_REG_NONE for a computed item */
	enum app_modbus_format fmt;
	float div; /**< value = raw / div (integer formats; ignored for float formats) */
	enum app_driver_aggreg mode;
	float mul;                 /**< CBOR int32 = value * mul, must match $div in codec */
	uint32_t key;              /**< CODEC_KEY_E_DEVICES__<GROUP>__<ITEM> */
	uint32_t key_measurements; /**< CODEC_KEY_E_DEVICES__<GROUP>__<ITEM>__MEASUREMENTS */
	enum app_driver_lrw lrw;   /**< LoRaWAN encoding */
	float lrw_mul;             /**< LoRaWAN value = value * lrw_mul (0 = see lrw) */
};

/**
 * @brief Item table entry
 *
 * _key is the codec key of the item without the __MEASUREMENTS suffix, the measurements
 * key is token-pasted from it. The codec header must be included where this is used.
 */
#define APP_DRIVER_ITEM(_name, _unit, _reg, _fmt, _div, _mode, _mul, _key)                         \
	APP_DRIVER_ITEM_LRW(_name, _unit, _reg, _fmt, _div, _mode, _mul, _key,                     \
			    APP_DRIVER_LRW_AUTO, 0.f)

/** Item table entry with an explicit LoRaWAN encoding */
#define APP_DRIVER_ITEM_LRW(_name, _unit, _reg, _fmt, _div, _mode, _mul, _key, _lrw, _lrw_mul)     \
	{                                                                                          \
		.name = _name,                                                                     \
		.unit = _unit,                                                                     \
		.reg = _reg,                                                                       \
		.fmt = _fmt,                                                                       \
		.div = _div,                                                                       \
		.mode = _mode,                                                                     \
		.mul = _mul,                                                                       \
		.key = _key,                                                                       \
		.key_measurements = _key##__MEASUREMENTS,                                          \
		.lrw = _lrw,                                                                       \
		.lrw_mul = _lrw_mul,                                                               \
	}

/** Ring of raw sample records */
struct app_driver_records {
	uint8_t *buf;
	size_t size;   /**< Record size */
	uint16_t cap;  /**< Capacity in records (0 = records are not kept) */
	uint32_t head; /**< Sequence number of the next record */
	uint32_t tail; /**< Sequence number of the oldest record */
};

/** Running state of one item in the open window */
struct app_driver_item_state {
	float last; /**< Last valid sample (NAN if none yet) */
	float min;
	float max;
	float sum;
	float sum_sin;
	float sum_cos;
	uint16_t count; /**< Valid samples in the open window */
	float *samples; /**< Median buffer (STATS) */
	void *windows;  /**< Ring of closed windows: struct ctr_data_aggreg (STATS) or float */
};

/** Item store */
struct app_driver_items {
	const struct app_driver_item *desc;
	struct app_driver_item_state *state;
	uint16_t count;            /**< Number of items */
	uint16_t samples_cap;      /**< Median buffer per item */
	uint16_t win_cap;          /**< Capacity of the window ring (0 = no windows kept) */
	uint32_t win_head;         /**< Sequence number of the next window */
	uint32_t win_tail;         /**< Sequence number of the oldest window */
	int64_t base_ts;           /**< Timestamp of window sequence number 0 */
	int64_t last_close_ts;     /**< RTC time of the last close (Unix seconds) */
	int64_t last_close_uptime; /**< Uptime of the last close (milliseconds) */
	int interval;              /**< Window length in seconds */
};

/** Device slot */
struct app_driver_slot {
	int idx;                             /**< Configuration slot (device-N) */
	uint8_t addr;                        /**< Modbus address from configuration */
	const struct app_device_driver *drv; /**< NULL = slot not in use */
	struct k_mutex lock;
	bool initialized; /**< Driver init done */
	void *ctx;        /**< Driver private state */
	struct app_driver_records rec;
	struct app_driver_items items;
	uint32_t drops; /**< Records / windows dropped as the ring was full */
};

/** Pool demand of one slot, derived from its driver */
struct app_driver_demand {
	size_t ctx_size;    /**< Driver private state */
	size_t rec_size;    /**< Raw record size (0 = no records) */
	uint16_t rec_fixed; /**< Fixed record capacity with history (0 = derived from intervals) */
	const struct app_driver_item *items;
	size_t item_count;
};

/** Inputs of the pool plan */
struct app_driver_plan_params {
	int interval_sample;
	int interval_aggreg;
	int interval_report;
	bool keep_history; /**< Reports carry the history (LTE); otherwise only the latest */
};

/** Capacities and memory of one slot */
struct app_driver_slot_plan {
	uint16_t rec_cap;
	uint16_t win_cap;
	uint16_t samples_cap;
	size_t bytes;
};

/**
 * @brief Memory a slot needs for the given capacities
 */
size_t app_driver_slot_bytes(const struct app_driver_demand *demand, uint16_t rec_cap,
			     uint16_t win_cap, uint16_t samples_cap);

/**
 * @brief Plan the capacities of all slots in a pool
 *
 * The base capacity holds one report period (records: interval-report / interval-sample,
 * windows: interval-report / interval-aggreg, both + 1) or just the latest data without
 * keep_history. Leftover pool is spread over the rings for outage tolerance.
 *
 * @param demands Demand per slot, NULL for unused slots
 * @param count Number of slots
 * @param params Intervals and mode
 * @param pool_size Pool size in bytes
 * @param plans Output, one per slot
 * @retval 0 Base capacities fit
 * @retval -ENOSPC Capacities were reduced below the base to fit (plans are usable)
 * @retval -ENOMEM Even the minimum capacities do not fit (plans are not usable)
 */
int app_driver_pool_plan(const struct app_driver_demand *const *demands, size_t count,
			 const struct app_driver_plan_params *params, size_t pool_size,
			 struct app_driver_slot_plan *plans);

/**
 * @brief Set up the storage of a slot in the given memory
 *
 * @param slot Slot (lock, idx, addr, drv are left untouched)
 * @param demand Demand of the slot
 * @param plan Capacities (plan->bytes of mem are used)
 * @param mem Memory, 8-byte aligned
 * @param interval Window length in seconds (interval-aggreg)
 */
void app_driver_slot_setup(struct app_driver_slot *slot, const struct app_driver_demand *demand,
			   const struct app_driver_slot_plan *plan, void *mem, int interval);

static inline void app_driver_slot_lock(struct app_driver_slot *slot)
{
	k_mutex_lock(&slot->lock, K_FOREVER);
}

static inline void app_driver_slot_unlock(struct app_driver_slot *slot)
{
	k_mutex_unlock(&slot->lock);
}

/** Clamp a sequence range [*first, *end) to the entries in a ring [tail, head) */
static inline void app_driver_seq_clamp(uint32_t *first, uint32_t *end, uint32_t tail,
					uint32_t head)
{
	if ((int32_t)(*first - tail) < 0) {
		*first = tail;
	}

	if ((int32_t)(*end - head) > 0) {
		*end = head;
	}

	if ((int32_t)(*end - *first) < 0) {
		*end = *first;
	}
}

/* Records */

/** Append a record; a full ring drops its oldest record */
void app_driver_rec_push(struct app_driver_slot *slot, const void *rec);

static inline uint32_t app_driver_rec_first(const struct app_driver_slot *slot)
{
	return slot->rec.tail;
}

static inline uint32_t app_driver_rec_end(const struct app_driver_slot *slot)
{
	return slot->rec.head;
}

static inline uint32_t app_driver_rec_count(const struct app_driver_slot *slot)
{
	return slot->rec.head - slot->rec.tail;
}

/** Record with the given sequence number, NULL if not in the ring */
const void *app_driver_rec_get(const struct app_driver_slot *slot, uint32_t seq);

/** Drop the records before sequence number end */
void app_driver_rec_pop_until(struct app_driver_slot *slot, uint32_t end);

/* Items */

/** Add a sample of an item to the open window; NAN is ignored */
void app_driver_items_put(struct app_driver_slot *slot, size_t idx, float value);

/** Last valid sample of an item, NAN if none */
float app_driver_items_last(const struct app_driver_slot *slot, size_t idx);

/**
 * @brief Close the open window of every item
 *
 * Called on the expirations of the aggregation timer. Expirations that coalesced while the
 * work queue was blocked are windows without samples: ticks - 1 empty windows are inserted
 * before this one, so window timestamps stay base_ts + seq * interval; if the ring does not
 * hold them, the ring restarts. A close that is late but missed no expiration adds no window.
 * When the RTC moved differently from the uptime (the clock was set), the windows are kept
 * and the timeline is re-anchored at this window.
 *
 * @param ts Timestamp of the window end (Unix seconds)
 * @param uptime Uptime at the window end (milliseconds), only to detect a set clock
 * @param ticks Timer expirations since the previous call; 0 does nothing (the window stays
 *              open)
 */
void app_driver_items_aggreg(struct app_driver_slot *slot, int64_t ts, int64_t uptime,
			     uint32_t ticks);

static inline uint32_t app_driver_win_first(const struct app_driver_slot *slot)
{
	return slot->items.win_tail;
}

static inline uint32_t app_driver_win_end(const struct app_driver_slot *slot)
{
	return slot->items.win_head;
}

static inline uint32_t app_driver_win_count(const struct app_driver_slot *slot)
{
	return slot->items.win_head - slot->items.win_tail;
}

/** Timestamp of the window with the given sequence number */
static inline int64_t app_driver_win_ts(const struct app_driver_slot *slot, uint32_t seq)
{
	return slot->items.base_ts + (int64_t)seq * slot->items.interval;
}

/**
 * @brief Statistics of a closed window of a STATS item
 *
 * @return NULL if the item is not STATS or the window is not in the ring
 */
const struct ctr_data_aggreg *app_driver_win_stats(const struct app_driver_slot *slot, size_t idx,
						   uint32_t seq);

/**
 * @brief Single value of a closed window (STATS: average)
 *
 * @return NAN if the window is empty or not in the ring
 */
float app_driver_win_value(const struct app_driver_slot *slot, size_t idx, uint32_t seq);

/** Drop the windows before sequence number end */
void app_driver_win_pop_until(struct app_driver_slot *slot, uint32_t end);

/** Drop all records and closed windows */
void app_driver_slot_clear(struct app_driver_slot *slot);

/**
 * @brief Compute min / max / avg / median of samples (sorted in place)
 *
 * Any NAN sample or no samples gives NAN in all outputs.
 */
void app_driver_aggreg_samples(float *samples, size_t count, struct ctr_data_aggreg *out);

/**
 * @brief Item value on the CBOR wire: value * mul rounded to the nearest integer
 *
 * @retval true *out is set
 * @retval false value is NAN or value * mul is outside the int32 range (encoded as nil)
 */
bool app_driver_value_to_int32(float value, float mul, int32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_DRIVER_STORE_H_ */
