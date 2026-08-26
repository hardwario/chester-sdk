/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_config.h>
#include <chester/ctr_info.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_ble_tag, CONFIG_CTR_BLE_TAG_LOG_LEVEL);

#define SETTINGS_PFX "ble_tag"

/* Module name the config items report. ctr_config_show_item() prints
 * `<module> config <name> <value>`, and the shell root of this subsystem is `tag`,
 * so the items have to say `tag` for those lines to be valid set commands. The
 * settings subtree stays SETTINGS_PFX: h_set() and h_export() below are hand-rolled
 * and never derive a storage key from item->module, unlike ctr_config_h_export(). */
#define ITEM_MODULE "tag"

/* Per-slot storage key. Deliberately kept as the original hand-written `addr-N`
 * so that settings written by earlier firmware keep loading unchanged. */
#define SLOT_KEY_FMT  "addr-%zu"
#define SLOT_KEY_SIZE (sizeof("addr-") + 2)

#define SCAN_PARAMS_DEFAULTS                                                                       \
	{                                                                                          \
		.type = BT_LE_SCAN_TYPE_ACTIVE,                                                    \
		.options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,                                      \
		.interval = CTR_BLE_TAG_SCAN_MAX_TIME,                                             \
		.window = CTR_BLE_TAG_SCAN_MAX_TIME,                                               \
	}

struct config {
	bool enabled;
	int scan_interval;
	int scan_duration;
	uint8_t addr[CTR_BLE_TAG_COUNT][BT_ADDR_SIZE];
};

static struct config m_config_interim;

static struct config m_config;

/* Text mirror of m_config_interim.addr[], in human byte order - what the `slot-N`
 * config items show and parse. The binary array stays the single source of truth for
 * the radio, so nothing in storage, in the accept list or in any memcmp() site had to
 * change. Deliberately not a member of `struct config`: h_commit() memcpy()s that
 * whole struct into m_config, which would pay for the mirror twice.
 *
 * The two representations are only ever written together, by slot_set(). */
static char m_addr_str[CTR_BLE_TAG_COUNT][BT_ADDR_SIZE * 2 + 1];

/* Serialises edits to the pair above. Enrolment writes them from the Bluetooth RX thread
 * while the shell can be writing them too, and slot_set() has to update both together.
 * Held before m_tag_data_lock wherever both are needed. */
static K_MUTEX_DEFINE(m_config_lock);

/* Re-render the text mirror of one slot from its binary address. */
static void slot_addr_str_render(size_t slot)
{
	int ret;
	uint8_t swap_addr[BT_ADDR_SIZE];

	if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
		m_addr_str[slot][0] = '\0';
		return;
	}

	sys_memcpy_swap(swap_addr, m_config_interim.addr[slot], BT_ADDR_SIZE);

	ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, m_addr_str[slot], sizeof(m_addr_str[slot]),
			  false);
	if (ret < 0) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		m_addr_str[slot][0] = '\0';
	}
}

/* The only writer of a slot address. Pass NULL to clear the slot. Keeping both
 * representations in one place is the whole invariant behind the text mirror. */
static void slot_set(size_t slot, const uint8_t *addr)
{
	k_mutex_lock(&m_config_lock, K_FOREVER);

	if (addr == NULL) {
		memset(m_config_interim.addr[slot], 0, BT_ADDR_SIZE);
	} else {
		memcpy(m_config_interim.addr[slot], addr, BT_ADDR_SIZE);
	}

	slot_addr_str_render(slot);

	k_mutex_unlock(&m_config_lock);
}

/* Locate the slot holding a BLE-order address. Reports the index through `slot` when
 * given, so callers that only need "is it enrolled" can pass NULL. */
static bool slot_find_addr(const uint8_t *addr, size_t *slot)
{
	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (memcmp(m_config_interim.addr[i], addr, BT_ADDR_SIZE) == 0) {
			if (slot != NULL) {
				*slot = i;
			}
			return true;
		}
	}

	return false;
}

/* True when an edited slot has not been applied yet, so the committed configuration - and
 * with it the accept list the radio follows - still differs from what the shell reports. */
static bool slots_staged(void)
{
	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (memcmp(m_config.addr[i], m_config_interim.addr[i], BT_ADDR_SIZE) != 0) {
			return true;
		}
	}

	return false;
}

/* Slots of the active configuration that hold a tag. */
static size_t enrolled_count(void)
{
	size_t n = 0;

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (!ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			n++;
		}
	}

	return n;
}

/* Locate the lowest unoccupied slot, on the same terms as slot_find_addr(). */
static bool slot_find_free(size_t *slot)
{
	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[i])) {
			if (slot != NULL) {
				*slot = i;
			}
			return true;
		}
	}

	return false;
}

static int slot_parse_cb(const struct shell *shell, char *argv, const struct ctr_config_item *item);
static void print_slot(const struct shell *shell, size_t slot);

/* The shared CTR_CONFIG_ITEM_* macros hardcode `.module = SETTINGS_PFX`, which would
 * print `ble_tag config ...` and not match the `tag` shell root, so the items are
 * declared here with ITEM_MODULE instead. */
#define TAG_CONFIG_ITEM_BOOL(_name_d, _var, _help, _default)                                       \
	{                                                                                          \
		.module = ITEM_MODULE,                                                             \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_BOOL,                                                      \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.help = _help,                                                                     \
		.default_bool = _default,                                                          \
	}

#define TAG_CONFIG_ITEM_INT(_name_d, _var, _min, _max, _help, _default)                            \
	{                                                                                          \
		.module = ITEM_MODULE,                                                             \
		.name = _name_d,                                                                   \
		.type = CTR_CONFIG_TYPE_INT,                                                       \
		.variable = &_var,                                                                 \
		.size = sizeof(_var),                                                              \
		.min = _min,                                                                       \
		.max = _max,                                                                       \
		.help = _help,                                                                     \
		.default_int = _default,                                                           \
	}

#define TAG_CONFIG_ITEM_SLOT(_i, _)                                                                \
	{                                                                                          \
		.module = ITEM_MODULE,                                                             \
		.name = "slot-" #_i,                                                               \
		.type = CTR_CONFIG_TYPE_STRING,                                                    \
		.variable = m_addr_str[_i],                                                        \
		.size = sizeof(m_addr_str[_i]),                                                    \
		.help = "BLE tag address of slot " #_i                                             \
			" (12 hex digits, empty clears it, `tag apply` to take effect).",          \
		.default_string = "",                                                              \
		.parse_cb = slot_parse_cb,                                                         \
	}

/* clang-format off */
static const struct ctr_config_item m_config_items[] = {
	TAG_CONFIG_ITEM_BOOL("enabled", m_config_interim.enabled,
			     "Enable or disable the BLE tag scanner.", false),
	TAG_CONFIG_ITEM_INT("scan-interval", m_config_interim.scan_interval, 1, 86400,
			    "BLE tag scanner scan interval in seconds.", 300),
	TAG_CONFIG_ITEM_INT("scan-duration", m_config_interim.scan_duration, 1, 86400,
			    "BLE tag scanner scan duration in seconds.", 12),

	/* LISTIFY's separator has to stay parenthesised, which clang-format rewrites. */
	LISTIFY(CTR_BLE_TAG_COUNT, TAG_CONFIG_ITEM_SLOT, (,)),
};
/* clang-format on */

struct ble_tag_data {
	bool valid;
	int64_t timestamp;
	int16_t sensor_mask;
	int8_t rssi;
	float temperature;
	float humidity;
	float voltage;
	bool magnet_detected;
	bool moving;
	float movement_event_count;
	bool low_battery;
	float roll;
	float pitch;
};

static struct ble_tag_data m_tag_data[CTR_BLE_TAG_COUNT];
static K_MUTEX_DEFINE(m_tag_data_lock);

/* `tag discovery` prints each device the moment it is first seen, directly from discover_cb, so
 * no per-device sensor state needs to be retained. Only the address is kept, purely to dedupe
 * repeat adverts from the same tag during the scan window and to report a final unique count. */
static uint8_t m_discover_seen[CTR_BLE_TAG_DISCOVER_MAX_DEVICES][BT_ADDR_SIZE];
static size_t m_discover_seen_count;
static bool m_discover_full;
static K_MUTEX_DEFINE(m_discover_lock);

/* Own queue, not the system workqueue and not m_scan_work_q: the SCAN_JOB_ENROLL STOP step
 * calls apply(), which can block for several seconds waiting out a contended m_scan_lock.
 * That can never be allowed to stall the system workqueue (shared with everything, including
 * BT HCI), and scan_lock_acquire() must not run on m_scan_work_q -- its own handlers hold
 * m_scan_lock across two work items on that queue's single thread, and k_mutex is recursive
 * for its owner, so a lock attempt from that same thread would succeed immediately and wrongly
 * while a scan is genuinely still running. Only job_work_handler() is ever queued here. */
static struct k_work_q m_job_work_q;
static K_THREAD_STACK_DEFINE(m_job_work_q_stack, 2048);

/* An operator-driven scan needs two delays - one waiting for a scan opportunity, one for the
 * window itself - and nothing else, because the adverts arrive on the BT RX thread through the
 * per-kind callback. So a job runs as a two-step delayed work item, which supplies both delays
 * and the terminating timer, on the dedicated m_job_work_q above rather than on a thread whose
 * whole purpose would be to sleep. Sampling and enrolment can still finish early: their
 * callbacks reschedule the item, which replaces the pending window timeout. */
#define SCAN_JOB_LOCK_ATTEMPTS 30

enum scan_job_step {
	SCAN_JOB_STEP_START,
	SCAN_JOB_STEP_STOP,
};

enum scan_job_kind {
	SCAN_JOB_DISCOVERY,
	SCAN_JOB_SAMPLE,
	SCAN_JOB_ENROLL,
};

/* An operator-driven scan can run for up to CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MAX, so it is carried
 * out off the shell's thread: blocking a watchdog-monitored thread that long would trip it.
 * One job at a time, guarded by m_job_busy. */
struct scan_job {
	const struct shell *shell;
	enum scan_job_kind kind;
	int timeout_sec;
	int rssi_threshold; /* SCAN_JOB_ENROLL only */
};

static struct scan_job m_job;
static atomic_t m_job_busy;
static enum scan_job_step m_job_step;
static int m_job_attempts;

/* Enrolled slots a sample is waiting for. Computed by the START step, reported by STOP. */
static size_t m_job_target;

static void job_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_job_work, job_work_handler);

/* Called by a scan callback once the job has everything it was waiting for: rescheduling the
 * work item replaces its pending window timeout, so sampling and enrolment finish early instead
 * of burning the rest of the window. Safe from the BT RX thread. */
static void job_finish_early(void)
{
	k_work_reschedule_for_queue(&m_job_work_q, &m_job_work, K_NO_WAIT);
}

/* Tags enrolled by the running SCAN_JOB_ENROLL. */
static size_t m_enroll_count;

static K_MUTEX_DEFINE(m_scan_lock);

static struct k_work_q m_scan_work_q;
static K_THREAD_STACK_DEFINE(m_scan_work_q_stack, 2048);

static int16_t parse_data(struct net_buf_simple *buf, float *temperature, float *humidity,
			  float *voltage, bool *magnet_detected, bool *moving,
			  float *movement_event_count, bool *low_battery, float *roll, float *pitch)
{
	int16_t sensor_mask = 0;

	if (buf->len < 1) {
		LOG_DBG("Invalid length: %d", buf->len);
		return -EINVAL;
	}

	do {
		uint8_t len = net_buf_simple_pull_u8(buf);
		if (len > buf->len) {
			LOG_DBG("Invalid length: %d, needed: %d", buf->len, len);
			return -EINVAL;
		}

		uint8_t type = net_buf_simple_pull_u8(buf);
		if (type != 0xff) {
			net_buf_simple_pull_mem(buf, len - 1);
			continue;
		}

		uint16_t company_id = net_buf_simple_pull_le16(buf);
		if (company_id != 0x089a) {
			LOG_DBG("Invalid company ID: %x", company_id);
			return -EINVAL;
		}

		uint8_t version = net_buf_simple_pull_u8(buf);
		if (version != 0x01) {
			LOG_DBG("Invalid version: %d", version);
			return -EINVAL;
		}

		uint8_t data_flags = net_buf_simple_pull_u8(buf);

		int16_t temperature_;
		uint8_t humidity_;
		uint8_t voltage_;
		bool magnet_detected_;
		bool moving_;
		uint16_t movement_event_count_;
		int8_t roll_;
		int16_t pitch_;

		if (data_flags & BIT(0)) {
			if (buf->len < 2) {
				LOG_DBG("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint16_t));
				return -EINVAL;
			}

			temperature_ = net_buf_simple_pull_be16(buf);

			if (temperature) {
				*temperature = temperature_ / 100.0;
				LOG_DBG("Temperature: %.2f C", (double)*temperature);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE;
			}
		}

		if (data_flags & BIT(1)) {
			if (buf->len < 1) {
				LOG_DBG("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint8_t));
				return -EINVAL;
			}

			humidity_ = net_buf_simple_pull_u8(buf);

			if (humidity) {
				*humidity = humidity_;
				LOG_DBG("Humidity: %.0f %%", (double)*humidity);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_HUMIDITY;
			}
		}

		if (data_flags & BIT(2)) {
			magnet_detected_ = (data_flags & BIT(3)) != 0;

			if (magnet_detected) {
				*magnet_detected = magnet_detected_;
				LOG_DBG("magnet: %s",
					magnet_detected_ ? "detected" : "not detected");

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED;
			}
		}

		if (data_flags & BIT(4)) {
			if (buf->len < 2) {
				LOG_ERR("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint16_t));
				return -EINVAL;
			}

			uint16_t movement_data = net_buf_simple_pull_be16(buf);
			moving_ = movement_data & 0x8000 ? 1 : 0;
			movement_event_count_ = movement_data & 0x7fff;

			if (moving) {
				*moving = moving_ != 0;
				LOG_DBG("Moving: %s", moving_ ? "true" : "false");

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_MOVING;
			}

			if (movement_event_count) {
				*movement_event_count = movement_event_count_;
				LOG_DBG("Movement event count: %d", movement_event_count_);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT;
			}
		}

		if (data_flags & BIT(5)) {
			if (buf->len < 1) {
				LOG_ERR("Invalid length: %d, needed: %d", buf->len,
					3 * (sizeof(uint8_t)));
				return -EINVAL;
			}

			roll_ = net_buf_simple_pull_u8(buf);
			pitch_ = net_buf_simple_pull_be16(buf);

			if (roll) {
				*roll = roll_;
				LOG_DBG("Roll: %d", roll_);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_ROLL;
			}

			if (pitch) {
				*pitch = pitch_;
				LOG_DBG("Pitch: %d", pitch_);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_PITCH;
			}
		}

		if (low_battery) {
			*low_battery = data_flags & BIT(6);
			LOG_DBG("Low battery: %s", *low_battery ? "true" : "false");

			sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY;
		}

		if (data_flags & BIT(7)) {
			if (buf->len < 1) {
				LOG_ERR("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint8_t));
				return -EINVAL;
			}

			net_buf_simple_pull_mem(buf, buf->len - 1);

			voltage_ = net_buf_simple_pull_u8(buf);

			if (voltage) {
				*voltage = (voltage_ * 10) + 2000.0;
				LOG_DBG("Voltage: %.2f V", (double)*voltage);

				sensor_mask |= CTR_BLE_TAG_SENSOR_MASK_VOLTAGE;
			}
		}

	} while (buf->len);

	return sensor_mask ? sensor_mask : -EINVAL;
}

static void update_tag_data(const bt_addr_le_t *addr, int8_t rssi, bool interim,
			    int16_t sensor_mask, float temperature, float humidity, float voltage,
			    bool magnet_detected, bool moving, float movement_event_count,
			    bool low_battery, float roll, float pitch)
{
	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	size_t slot = 0;

	for (; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (!memcmp(addr->a.val,
			    interim ? m_config_interim.addr[slot] : m_config.addr[slot],
			    BT_ADDR_SIZE)) {
			break;
		}
	}

	if (slot == CTR_BLE_TAG_COUNT) {
		k_mutex_unlock(&m_tag_data_lock);
		return;
	}

	m_tag_data[slot].timestamp = k_uptime_get();
	m_tag_data[slot].sensor_mask = sensor_mask;
	m_tag_data[slot].rssi = rssi;
	m_tag_data[slot].temperature = temperature;
	m_tag_data[slot].humidity = humidity;
	m_tag_data[slot].voltage = voltage;
	m_tag_data[slot].magnet_detected = magnet_detected;
	m_tag_data[slot].moving = moving;
	m_tag_data[slot].movement_event_count = movement_event_count;
	m_tag_data[slot].low_battery = low_battery;
	m_tag_data[slot].roll = roll;
	m_tag_data[slot].pitch = pitch;

	m_tag_data[slot].valid = true;

	k_mutex_unlock(&m_tag_data_lock);
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	float temperature;
	float humidity;
	float voltage;
	bool magnet_detected;
	bool moving;
	float movement_event_count;
	bool low_battery;
	float roll;
	float pitch;

	int16_t sensor_mask =
		parse_data(buf, &temperature, &humidity, &voltage, &magnet_detected, &moving,
			   &movement_event_count, &low_battery, &roll, &pitch);

	if (sensor_mask < 0) {
		LOG_DBG("Failed to parse data");
		return;
	}

	update_tag_data(addr, rssi, false, sensor_mask, temperature, humidity, voltage,
			magnet_detected, moving, movement_event_count, low_battery, roll, pitch);
}

static void enroll_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		      struct net_buf_simple *buf)
{
	float temperature = NAN;
	float humidity = NAN;
	float voltage = NAN;
	bool magnet_detected;
	bool moving;
	float movement_event_count;
	bool low_battery;
	float roll;
	float pitch;

	int16_t sensor_mask =
		parse_data(buf, &temperature, &humidity, &voltage, &magnet_detected, &moving,
			   &movement_event_count, &low_battery, &roll, &pitch);

	if (sensor_mask < 0) {
		return;
	}

	/* Enrol every tag close enough to be the one in the operator's hand, not just the
	 * first: provisioning a room otherwise costs one scan window per tag. Held across
	 * the lookup and the write so a concurrent shell edit cannot claim the same slot. */
	k_mutex_lock(&m_config_lock, K_FOREVER);

	if (rssi >= m_job.rssi_threshold && !slot_find_addr(addr->a.val, NULL)) {
		size_t slot;

		if (slot_find_free(&slot)) {
			slot_set(slot, addr->a.val);
			m_enroll_count++;

			LOG_INF("Enrolled: %s", m_addr_str[slot]);
			shell_print(m_job.shell, "enrolled addr: %s -> slot %zu", m_addr_str[slot],
				    slot);
		}

		/* Nothing more can be enrolled, so do not hold the radio for the rest of
		 * the window. */
		if (!slot_find_free(NULL)) {
			job_finish_early();
		}
	}

	k_mutex_unlock(&m_config_lock);

	update_tag_data(addr, rssi, true, sensor_mask, temperature, humidity, voltage,
			magnet_detected, moving, movement_event_count, low_battery, roll, pitch);
}

static void discover_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			struct net_buf_simple *buf)
{
	int ret;

	float temperature = NAN;
	float humidity = NAN;
	float voltage = NAN;
	bool magnet_detected = false;
	bool moving = false;
	float movement_event_count = 0;
	bool low_battery = false;
	float roll = 0;
	float pitch = 0;

	int16_t sensor_mask =
		parse_data(buf, &temperature, &humidity, &voltage, &magnet_detected, &moving,
			   &movement_event_count, &low_battery, &roll, &pitch);

	if (sensor_mask < 0) {
		/* Not a HARDWARIO BLE tag advertisement */
		return;
	}

	k_mutex_lock(&m_discover_lock, K_FOREVER);

	for (size_t i = 0; i < m_discover_seen_count; i++) {
		if (memcmp(m_discover_seen[i], addr->a.val, BT_ADDR_SIZE) == 0) {
			/* Already printed on first sighting */
			k_mutex_unlock(&m_discover_lock);
			return;
		}
	}

	if (m_discover_seen_count >= CTR_BLE_TAG_DISCOVER_MAX_DEVICES) {
		/* Table full. Report it once rather than dropping in silence: otherwise the
		 * closing count reads as the complete picture and a consumer has no way to
		 * tell that nearby tags were left out. */
		if (!m_discover_full) {
			m_discover_full = true;
			shell_fprintf(m_job.shell, SHELL_NORMAL,
				      "reached the %d tag limit, further tags not reported\n",
				      CTR_BLE_TAG_DISCOVER_MAX_DEVICES);
		}

		k_mutex_unlock(&m_discover_lock);
		return;
	}

	memcpy(m_discover_seen[m_discover_seen_count], addr->a.val, BT_ADDR_SIZE);
	size_t index = m_discover_seen_count++;

	k_mutex_unlock(&m_discover_lock);

	uint8_t swap_addr[BT_ADDR_SIZE];
	char addr_str[BT_ADDR_SIZE * 2 + 1];

	sys_memcpy_swap(swap_addr, addr->a.val, BT_ADDR_SIZE);

	ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, addr_str, sizeof(addr_str), false);
	if (ret < 0) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		return;
	}

	/* Same segments and order as print_slot(), so a discovered tag and an enrolled one
	 * render identically and a consumer needs only one parser. Written out segment by
	 * segment rather than assembled in a buffer, which keeps 256 B off the stack. */
	const struct shell *shell = m_job.shell;

	shell_fprintf(shell, SHELL_NORMAL, "found %zu: addr: %s / rssi: %d dBm", index, addr_str,
		      rssi);

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
		shell_fprintf(shell, SHELL_NORMAL, " / voltage: %.2f V",
			      (double)(voltage / 1000.f));
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE) {
		shell_fprintf(shell, SHELL_NORMAL, " / temperature: %.2f C", (double)temperature);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_HUMIDITY) {
		shell_fprintf(shell, SHELL_NORMAL, " / humidity: %.2f %%", (double)humidity);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED) {
		shell_fprintf(shell, SHELL_NORMAL, " / magnetic sensor: %s",
			      magnet_detected ? "detected" : "not detected");
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVING) {
		shell_fprintf(shell, SHELL_NORMAL, " / moving: %s", moving ? "true" : "false");
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT) {
		shell_fprintf(shell, SHELL_NORMAL, " / movement event count: %d",
			      (int)movement_event_count);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_ROLL) {
		shell_fprintf(shell, SHELL_NORMAL, " / roll: %.2f", (double)roll);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_PITCH) {
		shell_fprintf(shell, SHELL_NORMAL, " / pitch: %.2f", (double)pitch);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY) {
		shell_fprintf(shell, SHELL_NORMAL, " / low battery: %s",
			      low_battery ? "true" : "false");
	}

	shell_fprintf(shell, SHELL_NORMAL, "\n");
}

static void start_scan_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_start_scan_work, start_scan_work_handler);

static void stop_scan_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_stop_scan_work, stop_scan_work_handler);

static void start_scan_work_handler(struct k_work *work)
{
	int ret;

	/* `enabled` can be turned off after this item was already scheduled, so the
	 * cancellation in apply() is not on its own enough to stop the cycle. */
	if (!m_config.enabled) {
		LOG_DBG("Scanner disabled, not starting scan");
		return;
	}

	/* An empty accept list matches no advertiser, so with no tag enrolled the window would
	 * spend scan_duration of radio time to learn nothing. Stop the cycle rather than
	 * rescheduling: the active configuration only ever changes in apply() and in h_commit()
	 * (the latter via settings_load() at boot), and both apply() itself and init() right
	 * after settings_load() schedule this item afterwards, so enrolling a tag starts it
	 * again. */
	if (enrolled_count() == 0) {
		LOG_DBG("No tag enrolled, not starting scan");
		return;
	}

	LOG_DBG("Starting scan...");

	k_mutex_lock(&m_scan_lock, K_FOREVER);

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;

	ret = bt_le_scan_start(&param, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);

		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work,
					  K_SECONDS(m_config.scan_interval));

		k_mutex_unlock(&m_scan_lock);

		return;
	}

	k_work_schedule_for_queue(&m_scan_work_q, &m_stop_scan_work,
				  K_SECONDS(m_config.scan_duration));

	LOG_DBG("Scan started");
}

static void stop_scan_work_handler(struct k_work *work)
{
	int ret;

	LOG_DBG("Stopping scan...");

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
	}

	k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work,
				  K_SECONDS(m_config.scan_interval));

	LOG_DBG("Scan stopped");

	k_mutex_unlock(&m_scan_lock);
}

/* Arm the controller's accept list from the active configuration. The controller
 * rejects this while a scan is filtering on the list, so the caller must hold
 * m_scan_lock, which is held for the whole of every scan window. */
static int accept_list_rebuild(void)
{
	int ret;

	ret = bt_le_filter_accept_list_clear();
	if (ret) {
		LOG_ERR("Call `bt_le_filter_accept_list_clear` failed: %d", ret);
		return ret;
	}

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			continue;
		}

		bt_addr_le_t addr = {
			.type = BT_ADDR_LE_PUBLIC,
		};

		memcpy(addr.a.val, m_config.addr[slot], BT_ADDR_SIZE);

		ret = bt_le_filter_accept_list_add(&addr);
		if (ret) {
			LOG_ERR("Call `bt_le_filter_accept_list_add` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

/* Take m_scan_lock, waiting out an in-progress scan window. Shared by every command that
 * needs the radio or the accept list to itself, from the shell thread or the scan-job
 * thread. Must NOT be called from m_scan_work_q: that queue's own handlers hold this lock
 * across two work items, and k_mutex is recursive for its owner, so it would succeed
 * immediately - and wrongly - while a scan is running. */
static int scan_lock_acquire(const struct shell *shell)
{
	int ret;

	for (int i = 30; i; i--) {
		ret = k_mutex_lock(&m_scan_lock, K_NO_WAIT);
		if (!ret) {
			return 0;
		}

		if (i == 1) {
			shell_print(shell, "waiting timed out");
			return -EBUSY;
		}

		shell_print(shell, "waiting for scan opportunity...");

		k_sleep(K_SECONDS(2));
	}

	return -EBUSY;
}

/* Make the edited configuration live without the cold reboot that `config save`
 * performs: refresh the active copy, re-arm the accept list from it, and bring the
 * periodic scan into line with `enabled`. */
static int apply(const struct shell *shell)
{
	int ret;

	ret = scan_lock_acquire(shell);
	if (ret) {
		return ret;
	}

	k_mutex_lock(&m_config_lock, K_FOREVER);
	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		/* Readings belong to the address that produced them, so a slot taken over
		 * by a different tag must not inherit them. A slot that was empty is left
		 * alone: it holds nothing to inherit, and enrolment stores the advert it
		 * enrolled from before getting here. */
		if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			continue;
		}

		if (memcmp(m_config.addr[slot], m_config_interim.addr[slot], BT_ADDR_SIZE) != 0) {
			memset(&m_tag_data[slot], 0, sizeof(m_tag_data[slot]));
		}
	}

	memcpy(&m_config, &m_config_interim, sizeof(m_config));

	k_mutex_unlock(&m_tag_data_lock);
	k_mutex_unlock(&m_config_lock);

	ret = accept_list_rebuild();

	k_mutex_unlock(&m_scan_lock);

	if (ret) {
		return ret;
	}

	/* The scan work is only ever scheduled while enabled, so a change of `enabled`
	 * has to arm or disarm it here. k_work_schedule_for_queue() leaves an already
	 * pending item alone, so an ongoing scan cycle is not disturbed. Done after
	 * unlocking so the queue does not immediately block on m_scan_lock. */
	if (m_config.enabled) {
		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work, K_NO_WAIT);
	} else {
		(void)k_work_cancel_delayable(&m_start_scan_work);
	}

	return 0;
}

static int cmd_apply(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = apply(shell);
	if (ret) {
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

int ctr_ble_tag_is_addr_empty(const uint8_t addr[BT_ADDR_SIZE])
{
	static const uint8_t empty_addr[BT_ADDR_SIZE] = {0};
	return memcmp(addr, empty_addr, BT_ADDR_SIZE) == 0;
}

int ctr_ble_tag_read_cached(size_t slot, uint8_t addr[BT_ADDR_SIZE], int8_t *rssi, float *voltage,
			    float *temperature, float *humidity, bool *magnet_detected,
			    bool *moving, int *movement_event_count, float *roll, float *pitch,
			    bool *low_battery, int16_t *sensor_mask, bool *valid)
{
	if (slot >= CTR_BLE_TAG_COUNT) {
		return -EINVAL;
	}

	if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
		return -ENOENT;
	}

	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	if (addr) {
		sys_memcpy_swap(addr, m_config.addr[slot], BT_ADDR_SIZE);
	}

	if (m_tag_data[slot].valid) {
		uint64_t now = k_uptime_get();
		if (now > m_tag_data[slot].timestamp + m_config.scan_interval * 1000 *
							       CTR_BLE_TAG_DATA_TIMEOUT_INTERVALS) {

			m_tag_data[slot].valid = false;
		}
	}

	if (rssi) {
		*rssi = m_tag_data[slot].rssi;
	}

	if (voltage && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
		*voltage = m_tag_data[slot].voltage / 1000.f;
	}

	if (temperature && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE) {
		*temperature = m_tag_data[slot].temperature;
	}

	if (humidity && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_HUMIDITY) {
		*humidity = m_tag_data[slot].humidity;
	}

	if (magnet_detected &&
	    m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED) {
		*magnet_detected = m_tag_data[slot].magnet_detected;
	}

	if (moving && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVING) {
		*moving = m_tag_data[slot].moving;
	}

	if (movement_event_count &&
	    m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT) {
		*movement_event_count = m_tag_data[slot].movement_event_count;
	}

	if (roll && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_ROLL) {
		*roll = m_tag_data[slot].roll;
	}

	if (pitch && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_PITCH) {
		*pitch = m_tag_data[slot].pitch;
	}

	if (low_battery && m_tag_data[slot].sensor_mask & CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY) {
		*low_battery = m_tag_data[slot].low_battery;
	}

	if (sensor_mask) {
		*sensor_mask = m_tag_data[slot].sensor_mask;
	}

	if (valid) {
		*valid = m_tag_data[slot].valid;
	}

	k_mutex_unlock(&m_tag_data_lock);

	return 0;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET(_key, _var, _size)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, _var, len);                                          \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET("enabled", &m_config_interim.enabled, sizeof(m_config_interim.enabled));
	SETTINGS_SET("scan-interval", &m_config_interim.scan_interval,
		     sizeof(m_config_interim.scan_interval));
	SETTINGS_SET("scan-duration", &m_config_interim.scan_duration,
		     sizeof(m_config_interim.scan_duration));

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		char slot_key[SLOT_KEY_SIZE];

		snprintf(slot_key, sizeof(slot_key), SLOT_KEY_FMT, slot);

		SETTINGS_SET(slot_key, m_config_interim.addr[slot],
			     sizeof(m_config_interim.addr[slot]));
	}

#undef SETTINGS_SET

	return 0;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	/* h_set() only fills the binary addresses, so the text mirror is brought up to
	 * date here - the one place besides slot_set() where a slot address changes. */
	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		slot_addr_str_render(slot);
	}

	memcpy(&m_config, &m_config_interim, sizeof(m_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
#define EXPORT_FUNC(_key, _var, _size)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

	EXPORT_FUNC("enabled", &m_config_interim.enabled, sizeof(m_config_interim.enabled));
	EXPORT_FUNC("scan-interval", &m_config_interim.scan_interval,
		    sizeof(m_config_interim.scan_interval));
	EXPORT_FUNC("scan-duration", &m_config_interim.scan_duration,
		    sizeof(m_config_interim.scan_duration));

#undef EXPORT_FUNC

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		char slot_key[sizeof(SETTINGS_PFX "/") + SLOT_KEY_SIZE];

		snprintf(slot_key, sizeof(slot_key), SETTINGS_PFX "/" SLOT_KEY_FMT, slot);

		(void)export_func(slot_key, m_config_interim.addr[slot],
				  sizeof(m_config_interim.addr[slot]));
	}

	return 0;
}

/* Validate and assign one `slot-N` item. Accepts bare or separated hex in either
 * case, and an empty string to clear the slot. */
static int slot_parse_cb(const struct shell *shell, char *argv, const struct ctr_config_item *item)
{
	int ret;
	uint8_t addr[BT_ADDR_SIZE];
	char norm[BT_ADDR_SIZE * 2 + 1];
	size_t norm_len = 0;

	const size_t slot = ((char *)item->variable - m_addr_str[0]) / sizeof(m_addr_str[0]);

	if (argv[0] == '\0') {
		slot_set(slot, NULL);
		return 0;
	}

	/* ctr_hex2buf() takes bare hex digits, so the separators of a conventionally
	 * written MAC have to be dropped before it sees the value. */
	for (const char *p = argv; *p != '\0'; p++) {
		if (*p == ':' || *p == '-') {
			continue;
		}

		if (norm_len >= sizeof(norm) - 1) {
			norm_len = sizeof(norm); /* Too long - reported below. */
			break;
		}

		norm[norm_len++] = tolower((int)*p);
	}

	if (norm_len != BT_ADDR_SIZE * 2) {
		shell_error(shell, "expected %d hex digits", BT_ADDR_SIZE * 2);
		return -EINVAL;
	}

	norm[norm_len] = '\0';

	ret = ctr_hex2buf(norm, addr, BT_ADDR_SIZE, false);
	if (ret < 0) {
		shell_error(shell, "expected %d hex digits", BT_ADDR_SIZE * 2);
		return -EINVAL;
	}

	sys_mem_swap(addr, BT_ADDR_SIZE);

	/* An all-zero address is the empty-slot sentinel, so it cannot be assigned. */
	if (ctr_ble_tag_is_addr_empty(addr)) {
		shell_error(shell, "use \"\" to clear a slot");
		return -EINVAL;
	}

	/* Held across the whole check-then-write below, not just slot_set()'s own write, so
	 * a concurrent enroll_cb() (Bluetooth RX thread) or another shell edit cannot observe
	 * the pre-write state and claim the same address in a different slot. */
	k_mutex_lock(&m_config_lock, K_FOREVER);

	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (i == slot) {
			continue;
		}

		if (memcmp(m_config_interim.addr[i], addr, BT_ADDR_SIZE) == 0) {
			k_mutex_unlock(&m_config_lock);
			shell_error(shell, "tag addr %s already assigned to slot %zu", norm, i);
			return -EEXIST;
		}
	}

	slot_set(slot, addr);

	k_mutex_unlock(&m_config_lock);

	return 0;
}

/* `tag config <name> <value>` prints nothing on success (an invalid value still
 * prints shell_error), matching the convention already used by ctr_ble.c and
 * ctr_lte_v2_config.c. This is deliberate, not an oversight: Manager-App's
 * config editor writes a slot with ChesterController.runShell(), which treats
 * 600 ms of quiet as "done" rather than waiting for a command-succeeded
 * marker -- chester_tag_controller.dart's comment on its `tag config slot-*`
 * read says so explicitly ("print no end-of-command marker, so these are
 * quiet-window reads, not marker reads"), and its save path writes each slot
 * the same way. The long-running commands below (tag discovery/tag list
 * sample/tag enroll) are the ones that must end in a command-succeeded /
 * command-failed / command-aborted line, because they can go quiet for
 * legitimate reasons for up to CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MAX and the app's
 * quiet-window reader would otherwise return early. */
static int cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	return ctr_config_cmd_config(m_config_items, ARRAY_SIZE(m_config_items), shell, argc, argv);
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	for (size_t i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_show_item(shell, &m_config_items[i]);
	}

	return 0;
}

/* Backs the deprecated `tag config devices list` alias only - the slot list that
 * `config show` reports now comes from the `slot-N` items. */
static void print_tag_list(const struct shell *shell)
{
	int ret;

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		uint8_t swap_addr[BT_ADDR_SIZE];
		char addr_str[BT_ADDR_SIZE * 2 + 1];

		sys_memcpy_swap(swap_addr, m_config_interim.addr[slot], BT_ADDR_SIZE);

		ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, addr_str, BT_ADDR_SIZE * 2 + 1, false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			return;
		}

		shell_print(shell, "tag config devices addr %zu %s", slot, addr_str);
	}
}

int ctr_ble_tag_enable(bool enabled)
{
	m_config_interim.enabled = enabled;
	return 0;
}

int ctr_ble_tag_set_scan_interval(int scan_interval)
{
	if (scan_interval < 1 || scan_interval > 86400) {
		return -EINVAL;
	}

	m_config_interim.scan_interval = scan_interval;

	return 0;
}

int ctr_ble_tag_get_scan_interval(void)
{
	return m_config_interim.scan_interval;
}

int ctr_ble_tag_set_scan_duration(int scan_duration)
{
	if (scan_duration < 1 || scan_duration > 86400) {
		return -EINVAL;
	}

	m_config_interim.scan_duration = scan_duration;

	return 0;
}

int ctr_ble_tag_get_scan_duration(void)
{
	return m_config_interim.scan_duration;
}

int ctr_ble_tag_add(char *addr_str)
{
	int ret;

	uint8_t addr[BT_ADDR_SIZE];

	ret = ctr_hex2buf(addr_str, addr, BT_ADDR_SIZE, false);
	if (ret < 0) {
		LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
		return ret;
	}

	sys_mem_swap(addr, BT_ADDR_SIZE);

	int8_t empty_slot = -1;

	/* Held across the whole check-then-write below, not just slot_set()'s own write,
	 * so a concurrent enroll_cb() (Bluetooth RX thread) or slot_parse_cb() (shell) cannot
	 * observe the pre-write state and claim the same address or slot. */
	k_mutex_lock(&m_config_lock, K_FOREVER);

	/* Every slot has to be inspected before a free one is picked, otherwise an
	 * address already held in a later slot is not detected and gets added twice. */
	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (memcmp(m_config_interim.addr[slot], addr, BT_ADDR_SIZE) == 0) {
			LOG_WRN("Tag addr %s already exists", addr_str);
			k_mutex_unlock(&m_config_lock);
			return -EEXIST;
		}

		if (empty_slot == -1 && ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
			empty_slot = slot;
		}
	}

	if (empty_slot != -1) {
		slot_set(empty_slot, addr);
		k_mutex_unlock(&m_config_lock);
		LOG_INF("Tag addr %s added to slot %d", addr_str, empty_slot);
		return empty_slot;
	}

	k_mutex_unlock(&m_config_lock);

	LOG_ERR("No slot available");

	return -ENOSPC;
}

static int cmd_config_add_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = ctr_ble_tag_add(argv[1]);
	if (ret == -EEXIST) {
		shell_print(shell, "tag addr %s already exists", argv[1]);
		shell_error(shell, "command failed");
		return -EEXIST;
	} else if (ret == -ENOSPC) {
		shell_print(shell, "no slot available");
		shell_error(shell, "command failed");
		return -ENOSPC;
	}

	shell_print(shell, "tag addr %s added to slot %d", argv[1], ret);

	/* Adding a tag is an action, not a staged edit, so it takes effect now. */
	ret = apply(shell);
	if (ret) {
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

int ctr_ble_tag_remove_addr(char *addr_str)
{
	int ret;

	uint8_t addr[BT_ADDR_SIZE];

	ret = ctr_hex2buf(addr_str, addr, BT_ADDR_SIZE, false);
	if (ret < 0) {
		LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
		return ret;
	}

	sys_mem_swap(addr, BT_ADDR_SIZE);

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (memcmp(m_config_interim.addr[slot], addr, BT_ADDR_SIZE) == 0) {
			slot_set(slot, NULL);
			LOG_INF("Tag addr %s removed from slot %zu", addr_str, slot);
			return slot;
		}
	}

	LOG_WRN("tag addr %s not found", addr_str);

	return -ENOENT;
}

int ctr_ble_tag_remove_slot(size_t slot)
{
	if (slot >= CTR_BLE_TAG_COUNT) {
		return -EINVAL;
	}

	if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
		LOG_WRN("slot is already empty");
		return -ENOENT;
	}

	slot_set(slot, NULL);

	return 0;
}

static int cmd_config_remove_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	/* An argument longer than 2 characters is a MAC address, anything shorter is
	 * a slot index - a slot index can never exceed 2 digits. */
	if (strlen(argv[1]) > 2) {
		ret = ctr_ble_tag_remove_addr(argv[1]);
		if (ret == -ENOENT) {
			shell_print(shell, "tag addr %s not found", argv[1]);
			shell_error(shell, "command failed");
			return -ENOENT;
		} else if (ret < 0) {
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell, "tag addr %s removed", argv[1]);
	} else {
		int slot = strtol(argv[1], NULL, 10);
		if (slot < 0 || slot >= CTR_BLE_TAG_COUNT) {
			shell_print(shell, "slot %d out of range", slot);
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		ret = ctr_ble_tag_remove_slot(slot);
		if (ret == -ENOENT) {
			shell_print(shell, "slot is already empty");
			return -ENOENT;
		}

		shell_print(shell, "slot %d removed", slot);
	}

	ret = apply(shell);
	if (ret) {
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_config_list_tags(const struct shell *shell, size_t argc, char **argv)
{
	print_tag_list(shell);

	shell_print(shell, "command succeeded");

	return 0;
}

void ctr_ble_tag_remove_all(void)
{
	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		slot_set(slot, NULL);
	}
}

static int cmd_config_clear_tags(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ctr_ble_tag_remove_all();

	ret = apply(shell);
	if (ret) {
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "command succeeded");

	return 0;
}

/* Enrolled slots that have not reported since the running sample job cleared them. */
static size_t sample_outstanding(void)
{
	size_t n = 0;

	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			continue;
		}

		if (!m_tag_data[slot].valid) {
			n++;
		}
	}

	k_mutex_unlock(&m_tag_data_lock);

	return n;
}

/* Set once a sample job has discarded the previous readings. Until then an advert must not
 * be allowed to complete the job: nothing has been invalidated yet, so the outstanding
 * count could already be zero and the job would return the very data it meant to refresh. */
static bool m_sample_armed;

static void sample_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		      struct net_buf_simple *buf)
{
	scan_cb(addr, rssi, adv_type, buf);

	/* Waiting for fresh values from every enrolled tag is the whole point of sampling,
	 * so finish the moment the last one reports rather than sitting out the timeout. */
	if (m_sample_armed && sample_outstanding() == 0) {
		job_finish_early();
	}
}

/* Discard the readings of every enrolled slot, so that what the sample reports can only be
 * data that arrived during this scan. Deliberately called only once the scan is known to
 * have started: these readings also feed ctr_ble_tag_read_cached(), so a command that
 * cannot go ahead must not cost the application a reporting cycle. */
static void sample_invalidate(void)
{
	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			continue;
		}

		m_tag_data[slot].valid = false;
	}

	k_mutex_unlock(&m_tag_data_lock);
}

/* Both steps of every operator-driven scan, run on the dedicated m_job_work_q. START waits for
 * a scan opportunity, retrying by rescheduling itself rather than sleeping, then opens the scan
 * and arms the window. STOP closes it and reports according to the job kind -- for
 * SCAN_JOB_ENROLL that includes calling apply(), which can block this queue for several seconds;
 * see m_job_work_q's own comment for why that is safe here and would not be on either of the
 * other two queues in this file. */
static void job_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	const struct shell *shell = m_job.shell;
	int ret;

	if (m_job_step == SCAN_JOB_STEP_STOP) {
		ret = bt_le_scan_stop();

		k_mutex_unlock(&m_scan_lock);

		if (ret) {
			LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
			shell_error(shell, "command failed");
			atomic_clear(&m_job_busy);
			return;
		}

		switch (m_job.kind) {
		case SCAN_JOB_DISCOVERY: {
			k_mutex_lock(&m_discover_lock, K_FOREVER);
			size_t found = m_discover_seen_count;
			k_mutex_unlock(&m_discover_lock);

			shell_print(shell, "scan finished");
			shell_print(shell, "discovered %zu BLE tag(s)", found);
			break;
		}

		case SCAN_JOB_SAMPLE:
			for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
				print_slot(shell, slot);
			}

			shell_print(shell, "sampled %zu/%zu tag(s)",
				    m_job_target - sample_outstanding(), m_job_target);
			break;

		case SCAN_JOB_ENROLL:
			if (m_enroll_count == 0) {
				shell_print(shell, "enrollment timed out");
			}

			shell_print(shell, "enrolled %zu tag(s)", m_enroll_count);

			/* The scan lock was released above on purpose: apply() takes it itself,
			 * and k_mutex is recursive for its owner. */
			if (m_enroll_count > 0 && apply(shell) != 0) {
				shell_error(shell, "command failed");
				atomic_clear(&m_job_busy);
				return;
			}
			break;

		default:
			break;
		}

		shell_print(shell, "command succeeded");

		/* Cleared last - guards reuse of the work item by the next invocation. */
		atomic_clear(&m_job_busy);
		return;
	}

	if (k_mutex_lock(&m_scan_lock, K_NO_WAIT) != 0) {
		if (--m_job_attempts <= 0) {
			shell_print(shell, "waiting timed out");
			shell_error(shell, "command failed");
			atomic_clear(&m_job_busy);
			return;
		}

		shell_print(shell, "waiting for scan opportunity...");
		k_work_reschedule_for_queue(&m_job_work_q, &m_job_work, K_SECONDS(2));
		return;
	}

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;
	bt_le_scan_cb_t *cb;

	m_job_target = 0;

	switch (m_job.kind) {
	case SCAN_JOB_DISCOVERY:
		k_mutex_lock(&m_discover_lock, K_FOREVER);
		m_discover_seen_count = 0;
		m_discover_full = false;
		k_mutex_unlock(&m_discover_lock);

		/* Every nearby tag, not just the enrolled ones. */
		param.options = BT_LE_SCAN_OPT_NONE;
		cb = discover_cb;

		shell_print(shell, "discovering all nearby BLE tags for %d s...",
			    m_job.timeout_sec);
		break;

	case SCAN_JOB_SAMPLE:
		m_job_target = enrolled_count();
		m_sample_armed = false;
		cb = sample_cb;

		if (m_job_target > 0) {
			shell_print(shell, "sampling %zu enrolled tag(s), up to %d s...",
				    m_job_target, m_job.timeout_sec);
		}
		break;

	case SCAN_JOB_ENROLL:
		m_enroll_count = 0;

		/* Unfiltered: a tag that is not enrolled yet cannot be on the accept list. */
		param.options = BT_LE_SCAN_OPT_NONE;
		cb = enroll_cb;

		shell_print(shell, "enrolling for up to %d s...", m_job.timeout_sec);
		break;

	default:
		/* Unreachable, but a job must never end without a terminator line: a
		 * streaming reader has nothing else to wait for. */
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		atomic_clear(&m_job_busy);
		return;
	}

	/* Nothing to wait for - skip the radio entirely rather than idle for the window. */
	if (m_job.kind == SCAN_JOB_SAMPLE && m_job_target == 0) {
		k_mutex_unlock(&m_scan_lock);
		shell_print(shell, "sampled 0/0 tag(s)");
		shell_print(shell, "command succeeded");
		atomic_clear(&m_job_busy);
		return;
	}

	ret = bt_le_scan_start(&param, cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		atomic_clear(&m_job_busy);
		return;
	}

	if (m_job.kind == SCAN_JOB_SAMPLE) {
		sample_invalidate();
		m_sample_armed = true;
	}

	m_job_step = SCAN_JOB_STEP_STOP;
	k_work_reschedule_for_queue(&m_job_work_q, &m_job_work, K_SECONDS(m_job.timeout_sec));
}

/* Parse an optional trailing timeout argument and hand the work to job_work_handler(). */
static int job_start(const struct shell *shell, enum scan_job_kind kind, int default_timeout,
		     const char *timeout_arg, int rssi_threshold)
{
	int timeout_sec = default_timeout;

	if (timeout_arg != NULL) {
		long v = strtol(timeout_arg, NULL, 10);

		if (v < CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MIN || v > CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MAX) {
			shell_print(shell, "timeout out of range [%d:%d]",
				    CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MIN,
				    CTR_BLE_TAG_SCAN_TIMEOUT_SEC_MAX);
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		timeout_sec = (int)v;
	}

	/* The scan runs asynchronously: this returns at once and the results, ending with a
	 * terminator line, are printed as the job progresses. */
	if (!atomic_cas(&m_job_busy, 0, 1)) {
		shell_print(shell, "another scan is already in progress");
		shell_error(shell, "command failed");
		return -EBUSY;
	}

	m_job.shell = shell;
	m_job.kind = kind;
	m_job.timeout_sec = timeout_sec;
	m_job.rssi_threshold = rssi_threshold;
	m_job_step = SCAN_JOB_STEP_START;
	m_job_attempts = SCAN_JOB_LOCK_ATTEMPTS;

	k_work_reschedule_for_queue(&m_job_work_q, &m_job_work, K_NO_WAIT);

	return 0;
}

static int cmd_scan(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!m_config.enabled) {
		shell_print(shell, "tag functionality is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	/* Undocumented alias of `tag discovery`, kept for one release. */
	if (argc >= 2) {
		if (strcmp(argv[1], "all") != 0) {
			shell_error(shell, "unknown parameter: %s", argv[1]);
			shell_help(shell);
			return -EINVAL;
		}

		return job_start(shell, SCAN_JOB_DISCOVERY,
				 CTR_BLE_TAG_DISCOVERY_TIMEOUT_SEC_DEFAULT,
				 argc == 3 ? argv[2] : NULL, 0);
	}

	ret = scan_lock_acquire(shell);
	if (ret) {
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "scanning...");

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;

	ret = bt_le_scan_start(&param, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	k_sleep(K_SECONDS(CTR_BLE_TAG_READ_TIMEOUT_SEC));

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	k_mutex_unlock(&m_scan_lock);

	shell_print(shell, "scan finished");

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		print_slot(shell, slot);
	}

	shell_print(shell, "command succeeded");

	return 0;
}

/* Print one slot as a single line. Every segment is either one `key: value` pair or one
 * bare marker, separated by ` / `, and an unoccupied slot is reported rather than
 * skipped, so a consumer can read the whole slot layout off this output. */
static void print_slot(const struct shell *shell, size_t slot)
{
	int ret;

	if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
		shell_print(shell, "slot %zu: empty", slot);
		return;
	}

	shell_fprintf(shell, SHELL_NORMAL, "slot %zu: addr: %s", slot, m_addr_str[slot]);

	/* m_tag_data[] is indexed by slot, so readings may only be attributed to this
	 * address while the committed configuration still agrees with the edited one.
	 * Otherwise a slot reassigned since the last `config save` would report the
	 * previous tag's readings under the new address. */
	if (memcmp(m_config.addr[slot], m_config_interim.addr[slot], BT_ADDR_SIZE) != 0) {
		shell_fprintf(shell, SHELL_NORMAL, " / not received data");
		shell_fprintf(shell, SHELL_NORMAL, "\n");
		return;
	}

	int8_t rssi = 0;
	float voltage = NAN;
	float temperature;
	float humidity;
	bool magnet_detected;
	bool moving;
	int movement_event_count;
	float roll;
	float pitch;
	bool low_battery;

	int16_t sensor_mask;
	bool valid;

	/* The address is taken from the configuration above, not from the cache. */
	ret = ctr_ble_tag_read_cached(slot, NULL, &rssi, &voltage, &temperature, &humidity,
				      &magnet_detected, &moving, &movement_event_count, &roll,
				      &pitch, &low_battery, &sensor_mask, &valid);
	if (ret) {
		shell_fprintf(shell, SHELL_NORMAL, " / not received data");
		shell_fprintf(shell, SHELL_NORMAL, "\n");
		return;
	}

	if (!valid) {
		shell_fprintf(shell, SHELL_NORMAL, " / not received data");

		if ((rssi < 0) || (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE)) {
			shell_fprintf(shell, SHELL_NORMAL, " / last received");
		}
	}

	if (rssi < 0) {
		shell_fprintf(shell, SHELL_NORMAL, " / rssi: %d dBm", rssi);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
		shell_fprintf(shell, SHELL_NORMAL, " / voltage: %.2f V", (double)voltage);
	}

	/* Everything past this point is only meaningful for a reading that is current. */
	if (!valid) {
		shell_fprintf(shell, SHELL_NORMAL, "\n");
		return;
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE) {
		shell_fprintf(shell, SHELL_NORMAL, " / temperature: %.2f C", (double)temperature);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_HUMIDITY) {
		shell_fprintf(shell, SHELL_NORMAL, " / humidity: %.2f %%", (double)humidity);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED) {
		shell_fprintf(shell, SHELL_NORMAL, " / magnetic sensor: %s",
			      magnet_detected ? "detected" : "not detected");
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVING) {
		shell_fprintf(shell, SHELL_NORMAL, " / moving: %s", moving ? "true" : "false");
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT) {
		shell_fprintf(shell, SHELL_NORMAL, " / movement event count: %d",
			      movement_event_count);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_ROLL) {
		shell_fprintf(shell, SHELL_NORMAL, " / roll: %.2f", (double)roll);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_PITCH) {
		shell_fprintf(shell, SHELL_NORMAL, " / pitch: %.2f", (double)pitch);
	}

	if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY) {
		shell_fprintf(shell, SHELL_NORMAL, " / low battery: %s",
			      low_battery ? "true" : "false");
	}

	shell_fprintf(shell, SHELL_NORMAL, "\n");
}

static int cmd_list_show(const struct shell *shell, size_t argc, char **argv)
{
	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		print_slot(shell, slot);
	}

	/* Which slots are occupied is configuration, not telemetry, so it stays
	 * readable with the scanner off - `enabled` is false on a factory device, and
	 * aborting here would leave a consumer unable to list the slot layout at all. */
	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_list_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	/* A staged slot cannot report: the scan follows the accept list, which is built from
	 * the committed configuration. Say so, rather than leaving it silently absent from
	 * the `sampled N/M` count while `tag list show` lists it. */
	if (slots_staged()) {
		shell_print(shell, "note: some slots are not applied yet, run `tag apply`");
	}

	return job_start(shell, SCAN_JOB_SAMPLE, CTR_BLE_TAG_SAMPLE_TIMEOUT_SEC_DEFAULT,
			 argc == 2 ? argv[1] : NULL, 0);
}

static int cmd_discovery(const struct shell *shell, size_t argc, char **argv)
{
	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	return job_start(shell, SCAN_JOB_DISCOVERY, CTR_BLE_TAG_DISCOVERY_TIMEOUT_SEC_DEFAULT,
			 argc == 2 ? argv[1] : NULL, 0);
}

static int cmd_enroll(const struct shell *shell, size_t argc, char **argv)
{
	int rssi_threshold = CTR_BLE_TAG_ENROLL_RSSI_THRESHOLD;

	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	if (argc >= 2) {
		long v = strtol(argv[1], NULL, 10);

		/* The threshold is what keeps enrolment to the tags physically at hand. */
		if (v < -128 || v > 0) {
			shell_print(shell, "invalid input, expected [-128:0]dbm");
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		rssi_threshold = (int)v;
	}

	if (!slot_find_free(NULL)) {
		shell_print(shell, "no slot available for enrollment");
		shell_error(shell, "command failed");
		return -ENOSPC;
	}

	return job_start(shell, SCAN_JOB_ENROLL, CTR_BLE_TAG_ENROLL_TIMEOUT_SEC,
			 argc == 3 ? argv[2] : NULL, rssi_threshold);
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag_config_devices,

	SHELL_CMD_ARG(add, NULL, "Add a device to the lowest free slot <MAC (12 hex digits)>.", cmd_config_add_tag, 2, 0),
	SHELL_CMD_ARG(list, NULL, "List all devices.", cmd_config_list_tags, 1, 0),
	SHELL_CMD_ARG(remove, NULL, "Remove a device <MAC (12 hex digits) | slot (at most 2 digits)>.", cmd_config_remove_tag, 2, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear all devices.", cmd_config_clear_tags, 1, 0),

	SHELL_SUBCMD_SET_END
);

/* `devices` is a deprecated alias of the `slot-N` config items, kept undocumented for
 * one release so existing scripts keep working. It is the only named subcommand here:
 * anything else after `tag config` finds no match and so falls through to cmd_config
 * (the shell's has_last_handler path), which is what routes `tag config <name> [value]`
 * and `tag config show` into ctr_config_cmd_config(). */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag_config,

	SHELL_CMD_ARG(devices, &sub_tag_config_devices, NULL, print_help, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag_list,

	SHELL_CMD_ARG(show, NULL, "Show every slot and its latest readings.", cmd_list_show, 1, 0),
	SHELL_CMD_ARG(sample, NULL, "Wait for fresh readings from every enrolled tag <timeout (1-300) s, default 60>.", cmd_list_sample, 1, 1),
	SHELL_CMD_ARG(add, NULL, "Add a tag to the lowest free slot <MAC (12 hex digits)>.", cmd_config_add_tag, 2, 0),
	SHELL_CMD_ARG(remove, NULL, "Remove a tag <MAC (12 hex digits) | slot (at most 2 digits)>.", cmd_config_remove_tag, 2, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear all slots.", cmd_config_clear_tags, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag,

	SHELL_CMD_ARG(config, &sub_tag_config,
				  "Configuration commands.",
				  cmd_config, 1, 2),
	SHELL_CMD_ARG(list, &sub_tag_list, "Enrolled tag commands.", print_help, 1, 0),
	SHELL_CMD_ARG(apply, NULL, "Make edited configuration live without a reboot.", cmd_apply, 1, 0),
	SHELL_CMD_ARG(discovery, NULL, "Report every nearby tag, enrolled or not <timeout (1-300) s, default 60>.", cmd_discovery, 1, 1),
	SHELL_CMD_ARG(enroll, NULL, "Enroll every nearby tag into the free slots <threshold (-128:0) dBm, default -40> <timeout (1-300) s, default 12>.", cmd_enroll, 1, 2),
	SHELL_CMD_ARG(read, NULL, "Read enrolled devices (12 seconds).", cmd_scan, 1, 2),

	/* Undocumented aliases of `tag list show` and `tag discovery`, kept for one release. */
	SHELL_CMD_ARG(show, NULL, NULL, cmd_list_show, 1, 0),

	SHELL_SUBCMD_SET_END
);

/* clang-format on */

SHELL_CMD_REGISTER(tag, &sub_tag, "BLE tag commands.", print_help);

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	memset(&m_tag_data, 0, sizeof(m_tag_data));

	for (size_t i = 0; i < ARRAY_SIZE(m_config_items); i++) {
		ctr_config_init_item(&m_config_items[i]);
	}

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	/* Registered under the shell root, not SETTINGS_PFX: `config modules` is how a
	 * consumer discovers which `<module> config` commands exist, and there is no
	 * `ble_tag config` command. */
	ctr_config_append_show(ITEM_MODULE, cmd_config_show);

	ret = settings_load();
	if (ret) {
		LOG_ERR("Call `settings_load` failed: %d", ret);
		return ret;
	}

	ret = accept_list_rebuild();
	if (ret) {
		LOG_ERR("Call `accept_list_rebuild` failed: %d", ret);
		return ret;
	}

	/* Started unconditionally: `enabled` can be turned on at run time and
	 * k_work_queue_start() cannot be called twice. The stack is reserved either
	 * way, so an idle queue costs nothing but the thread itself. */
	k_work_queue_start(&m_scan_work_q, m_scan_work_q_stack,
			   K_THREAD_STACK_SIZEOF(m_scan_work_q_stack),
			   K_LOWEST_APPLICATION_THREAD_PRIO, NULL);

	/* Separate from m_scan_work_q -- see m_job_work_q's own comment. Also started
	 * unconditionally: an operator-driven scan can be requested regardless of `enabled`. */
	k_work_queue_start(&m_job_work_q, m_job_work_q_stack,
			   K_THREAD_STACK_SIZEOF(m_job_work_q_stack),
			   K_LOWEST_APPLICATION_THREAD_PRIO, NULL);

	if (m_config.enabled) {
		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work, K_NO_WAIT);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
