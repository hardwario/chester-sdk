/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_trace_priv.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/retained_mem.h>
#include <zephyr/fatal.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_trace, CONFIG_CTR_TRACE_LOG_LEVEL);

#define TRACE_MEM_NODE DT_CHOSEN(ctr_trace_mem)

BUILD_ASSERT(DT_NODE_EXISTS(TRACE_MEM_NODE),
	     "ctr_trace: chosen `ctr,trace_mem` retained-memory node is missing "
	     "(enable the `ctr-trace` snippet or declare the node)");

/*
 * The fatal-error handler persists the fault marker from a context with
 * interrupts locked, so the retained_mem device must not use an internal mutex
 * (which would be illegal to take there). ctr_trace serializes its own accesses
 * with m_lock instead.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_RETAINED_MEM_MUTEXES),
	     "ctr_trace requires CONFIG_RETAINED_MEM_MUTEX_FORCE_DISABLE=y so the fatal-error "
	     "handler can persist the fault marker from any context");

/* Compile-time region size, taken from the retained-ram node's own reg. Must
 * equal the size the driver reports at runtime (parent memory-region size). */
#define TRACE_REGION_SIZE DT_REG_SIZE(TRACE_MEM_NODE)

BUILD_ASSERT(TRACE_REGION_SIZE >= sizeof(struct ctr_trace_header) +
					  (size_t)CONFIG_CTR_TRACE_RESET_REASON_COUNT *
						  sizeof(struct ctr_trace_boot_rec) +
					  2U * CTR_TRACE_MIN_LOG_SIZE,
	     "ctr_trace: retained region too small for the configured "
	     "CONFIG_CTR_TRACE_RESET_REASON_COUNT plus two log buffers");

static const struct device *m_dev = DEVICE_DT_GET(TRACE_MEM_NODE);

static struct ctr_trace_header m_hdr;
static struct ctr_trace_layout m_layout;
static bool m_ready;

K_MUTEX_DEFINE(m_lock);

/* ------------------------------------------------------------------------- */
/* Layout + fault-decision (pure)                                            */
/* ------------------------------------------------------------------------- */

static int layout_compute(size_t region_size, uint16_t reason_count,
			  struct ctr_trace_layout *layout)
{
	size_t hdr = sizeof(struct ctr_trace_header);
	size_t reasons = (size_t)reason_count * sizeof(struct ctr_trace_boot_rec);

	if (region_size < hdr + reasons + 2U * CTR_TRACE_MIN_LOG_SIZE) {
		return -ENOMEM;
	}

	size_t log = (region_size - hdr - reasons) / 2U;

	layout->reasons_off = (off_t)hdr;
	layout->reason_cap = reason_count;
	layout->primary_off = (off_t)(hdr + reasons);
	layout->secondary_off = (off_t)(hdr + reasons + log);
	layout->log_size = log;

	return 0;
}

static bool reset_is_fault(uint32_t reset_cause, bool fault_marker)
{
	if (fault_marker) {
		return true;
	}

	return (reset_cause & (RESET_WATCHDOG | RESET_CPU_LOCKUP)) != 0U;
}

/* ------------------------------------------------------------------------- */
/* Circular log region primitives                                            */
/* ------------------------------------------------------------------------- */

static int log_append(const struct device *dev, off_t data_off, size_t size, uint32_t *head,
		      uint32_t *count, const uint8_t *data, size_t len)
{
	int ret;

	if (size == 0 || len == 0) {
		return 0;
	}

	/* A single write larger than the buffer keeps only its last `size` bytes. */
	if (len >= size) {
		data += (len - size);
		len = size;
	}

	uint32_t h = *head;
	size_t first = MIN(len, size - h);

	ret = retained_mem_write(dev, data_off + (off_t)h, data, first);
	if (ret) {
		return ret;
	}

	if (len > first) {
		ret = retained_mem_write(dev, data_off, data + first, len - first);
		if (ret) {
			return ret;
		}
	}

	*head = (uint32_t)((h + len) % size);
	*count = (uint32_t)MIN((size_t)*count + len, size);

	return 0;
}

static int snapshot(const struct device *dev, off_t src_off, off_t dst_off, size_t size,
		    uint32_t head, uint32_t count, uint32_t *dst_len)
{
	int ret;
	uint8_t chunk[64];
	size_t remaining = MIN((size_t)count, size);

	*dst_len = (uint32_t)remaining;

	if (remaining == 0 || size == 0) {
		return 0;
	}

	uint32_t src = (uint32_t)(((size_t)head + size - count) % size);
	off_t dst = dst_off;

	while (remaining) {
		size_t take = MIN(remaining, sizeof(chunk));

		take = MIN(take, size - src);

		ret = retained_mem_read(dev, src_off + (off_t)src, chunk, take);
		if (ret) {
			return ret;
		}

		ret = retained_mem_write(dev, dst, chunk, take);
		if (ret) {
			return ret;
		}

		src = (uint32_t)((src + take) % size);
		dst += (off_t)take;
		remaining -= take;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/* Reset-record ring                                                         */
/* ------------------------------------------------------------------------- */

static int reason_append(const struct device *dev, off_t reasons_off, uint16_t capacity,
			 uint16_t *head, uint16_t *count, const struct ctr_trace_boot_rec *rec)
{
	int ret;

	if (capacity == 0) {
		return -ENOSPC;
	}

	off_t off = reasons_off + (off_t)(*head) * (off_t)sizeof(*rec);

	ret = retained_mem_write(dev, off, (const uint8_t *)rec, sizeof(*rec));
	if (ret) {
		return ret;
	}

	*head = (uint16_t)((*head + 1) % capacity);
	if (*count < capacity) {
		(*count)++;
	}

	return 0;
}

static int reason_get(const struct device *dev, off_t reasons_off, uint16_t capacity, uint16_t head,
		      uint16_t count, struct ctr_trace_boot_rec *out, uint16_t max,
		      uint16_t *copied)
{
	int ret;
	uint16_t n = MIN(count, max);

	if (capacity == 0) {
		*copied = 0;
		return 0;
	}

	uint16_t oldest = (uint16_t)((head + capacity - count) % capacity);

	for (uint16_t i = 0; i < n; i++) {
		uint16_t slot = (uint16_t)((oldest + i) % capacity);
		off_t off = reasons_off + (off_t)slot * (off_t)sizeof(*out);

		ret = retained_mem_read(dev, off, (uint8_t *)&out[i], sizeof(*out));
		if (ret) {
			return ret;
		}
	}

	*copied = n;

	return 0;
}

/* ------------------------------------------------------------------------- */
/* Retained header helpers                                                   */
/* ------------------------------------------------------------------------- */

static int store_header(void)
{
	return retained_mem_write(m_dev, 0, (const uint8_t *)&m_hdr, sizeof(m_hdr));
}

/* ------------------------------------------------------------------------- */
/* Logging backend                                                           */
/* ------------------------------------------------------------------------- */

static uint8_t m_output_buf[64];
static uint32_t m_log_format = LOG_OUTPUT_TEXT;

static int char_out(uint8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	if (!m_ready) {
		return length;
	}

	/* The lock is only held when reading through shell. We can drop data in
	 * that case. */
	if (k_mutex_lock(&m_lock, K_NO_WAIT) != 0) {
		return length;
	}

	log_append(m_dev, m_layout.primary_off, m_layout.log_size, &m_hdr.primary_head,
		   &m_hdr.primary_count, data, length);
	store_header();

	k_mutex_unlock(&m_lock);

	return length;
}

LOG_OUTPUT_DEFINE(m_log_output, char_out, m_output_buf, sizeof(m_output_buf));

static void process(const struct log_backend *const backend, union log_msg_generic *msg)
{
	ARG_UNUSED(backend);

	uint32_t flags = log_backend_std_get_flags() & ~LOG_OUTPUT_FLAG_COLORS;
	log_format_func_t func = log_format_func_t_get(m_log_format);

	func(&m_log_output, &msg->log, flags);
}

static void panic(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
}

static void dropped(const struct log_backend *const backend, uint32_t cnt)
{
	ARG_UNUSED(backend);

	log_backend_std_dropped(&m_log_output, cnt);
}

static int format_set(const struct log_backend *const backend, uint32_t log_type)
{
	ARG_UNUSED(backend);

	m_log_format = log_type;
	return 0;
}

static const struct log_backend_api log_backend_ctr_trace_api = {
	.process = process,
	.panic = panic,
	.dropped = dropped,
	.format_set = format_set,
};

/* autostart = false: the backend is enabled from init() only after the
 * previous boot's log has been snapshotted, so this boot's messages never
 * clobber the pre-fault content before it is captured. */
LOG_BACKEND_DEFINE(log_backend_ctr_trace, log_backend_ctr_trace_api, false);

/* ------------------------------------------------------------------------- */
/* Fatal-error handler                                                       */
/* ------------------------------------------------------------------------- */

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	ARG_UNUSED(esf);

	LOG_PANIC();
	LOG_ERR("Halting system");

	if (m_ready) {
		uint8_t one = 1;

		/* Best-effort, context-safe single-byte write (see the
		 * CONFIG_RETAINED_MEM_MUTEXES BUILD_ASSERT above). */
		(void)retained_mem_write(m_dev, offsetof(struct ctr_trace_header, fault_marker),
					 &one, sizeof(one));
	}
	k_fatal_halt(reason);

	CODE_UNREACHABLE;
}

/* ------------------------------------------------------------------------- */
/* Initialization                                                            */
/* ------------------------------------------------------------------------- */

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `%s` not ready", m_dev->name);
		return -ENODEV;
	}

	ssize_t region = retained_mem_size(m_dev);
	if (region < 0) {
		LOG_ERR("Call `retained_mem_size` failed: %d", (int)region);
		return (int)region;
	}

	ret = layout_compute((size_t)region, CONFIG_CTR_TRACE_RESET_REASON_COUNT, &m_layout);
	if (ret) {
		LOG_ERR("Retained region (%d B) too small: %d", (int)region, ret);
		return ret;
	}

	ret = retained_mem_read(m_dev, 0, (uint8_t *)&m_hdr, sizeof(m_hdr));
	if (ret) {
		LOG_ERR("Call `retained_mem_read` failed: %d", ret);
		return ret;
	}

	bool valid = (m_hdr.magic == CTR_TRACE_MAGIC) && (m_hdr.version == CTR_TRACE_VERSION);

	uint32_t cause = 0;
	if (IS_ENABLED(CONFIG_HWINFO)) {
		ret = hwinfo_get_reset_cause(&cause);
		if (ret) {
			LOG_WRN("Call `hwinfo_get_reset_cause` failed: %d", ret);
			cause = 0;
		} else {
			hwinfo_clear_reset_cause();
		}
	}

	if (!valid) {
		LOG_INF("Retained trace invalid, initializing (cold start)");
		memset(&m_hdr, 0, sizeof(m_hdr));
		m_hdr.magic = CTR_TRACE_MAGIC;
		m_hdr.version = CTR_TRACE_VERSION;
	} else if (reset_is_fault(cause, m_hdr.fault_marker)) {
		LOG_WRN("Previous boot faulted (cause 0x%08x, marker %u), capturing log snapshot",
			cause, m_hdr.fault_marker);
		ret = snapshot(m_dev, m_layout.primary_off, m_layout.secondary_off,
			       m_layout.log_size, m_hdr.primary_head, m_hdr.primary_count,
			       &m_hdr.secondary_len);
		if (ret) {
			LOG_ERR("Fault snapshot failed: %d", ret);
		} else {
			m_hdr.secondary_valid = 1;
		}
	}

	/* Record this boot. */
	if (m_hdr.boot_count < UINT16_MAX) {
		m_hdr.boot_count++;
	}

	struct ctr_trace_boot_rec rec = {0};
	rec.boot_num = m_hdr.boot_count;
	rec.reset_cause = cause;
	ret = reason_append(m_dev, m_layout.reasons_off, m_layout.reason_cap, &m_hdr.reason_head,
			    &m_hdr.reason_count, &rec);
	if (ret) {
		LOG_ERR("Recording boot reason failed: %d", ret);
	}

	/* Start this boot's log capture with a fresh primary ring; the pre-fault
	 * content (if any) is preserved in the secondary snapshot. */
	m_hdr.fault_marker = 0;
	m_hdr.primary_head = 0;
	m_hdr.primary_count = 0;

	ret = store_header();
	if (ret) {
		LOG_ERR("Call `store_header` failed: %d", ret);
		return ret;
	}

	m_ready = true;

	log_backend_enable(&log_backend_ctr_trace, NULL, CONFIG_LOG_MAX_LEVEL);

	LOG_INF("Boot #%u, reset cause 0x%08x", m_hdr.boot_count, cause);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_TRACE_INIT_PRIORITY);

/* ------------------------------------------------------------------------- */
/* Shell                                                                     */
/* ------------------------------------------------------------------------- */

#if defined(CONFIG_CTR_TRACE_SHELL)

struct reset_cause_name {
	uint32_t bit;
	const char *name;
};

static const struct reset_cause_name m_reset_cause_names[] = {
	{RESET_PIN, "pin"},
	{RESET_SOFTWARE, "software"},
	{RESET_BROWNOUT, "brownout"},
	{RESET_POR, "power-on"},
	{RESET_WATCHDOG, "watchdog"},
	{RESET_DEBUG, "debug"},
	{RESET_SECURITY, "security"},
	{RESET_LOW_POWER_WAKE, "low-power-wake"},
	{RESET_CPU_LOCKUP, "lockup"},
	{RESET_PARITY, "parity"},
	{RESET_PLL, "pll"},
	{RESET_CLOCK, "clock"},
	{RESET_HARDWARE, "hardware"},
	{RESET_USER, "user"},
	{RESET_TEMPERATURE, "temperature"},
};

static void shell_print_reset_cause(const struct shell *shell, uint32_t cause)
{
	if (cause == 0) {
		shell_fprintf(shell, SHELL_NORMAL, "none");
		return;
	}

	bool first = true;

	for (size_t i = 0; i < ARRAY_SIZE(m_reset_cause_names); i++) {
		if (cause & m_reset_cause_names[i].bit) {
			shell_fprintf(shell, SHELL_NORMAL, "%s%s", first ? "" : ",",
				      m_reset_cause_names[i].name);
			first = false;
		}
	}

	if (first) {
		shell_fprintf(shell, SHELL_NORMAL, "0x%08x", cause);
	}
}

static void shell_dump_ring(const struct shell *shell, off_t data_off, size_t size, uint32_t head,
			    uint32_t count)
{
	uint8_t buf[64];
	size_t remaining = MIN((size_t)count, size);

	if (remaining == 0) {
		shell_print(shell, "<empty>");
		return;
	}

	uint32_t pos = (uint32_t)(((size_t)head + size - remaining) % size);

	while (remaining) {
		size_t take = MIN(remaining, sizeof(buf));

		take = MIN(take, size - pos);
		retained_mem_read(m_dev, data_off + (off_t)pos, buf, take);
		shell_fprintf(shell, SHELL_NORMAL, "%.*s", (int)take, buf);
		pos = (uint32_t)((pos + take) % size);
		remaining -= take;
	}

	shell_fprintf(shell, SHELL_NORMAL, "\n");
}

static void shell_dump_linear(const struct shell *shell, off_t data_off, uint32_t len)
{
	uint8_t buf[64];
	off_t pos = data_off;
	uint32_t remaining = len;

	if (remaining == 0) {
		shell_print(shell, "<empty>");
		return;
	}

	while (remaining) {
		size_t take = MIN((size_t)remaining, sizeof(buf));

		retained_mem_read(m_dev, pos, buf, take);
		shell_fprintf(shell, SHELL_NORMAL, "%.*s", (int)take, buf);
		pos += (off_t)take;
		remaining -= (uint32_t)take;
	}

	shell_fprintf(shell, SHELL_NORMAL, "\n");
}

static int cmd_log_show(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	shell_dump_ring(shell, m_layout.primary_off, m_layout.log_size, m_hdr.primary_head,
			m_hdr.primary_count);
	k_mutex_unlock(&m_lock);

	return 0;
}

static int cmd_log_clear(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	m_hdr.primary_head = 0;
	m_hdr.primary_count = 0;
	store_header();
	k_mutex_unlock(&m_lock);

	shell_print(shell, "command succeeded");
	return 0;
}

static int cmd_fault_show(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	if (!m_hdr.secondary_valid) {
		shell_print(shell, "no fault snapshot");
	} else {
		shell_dump_linear(shell, m_layout.secondary_off, m_hdr.secondary_len);
	}
	k_mutex_unlock(&m_lock);

	return 0;
}

static int cmd_fault_clear(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	m_hdr.secondary_valid = 0;
	m_hdr.secondary_len = 0;
	store_header();
	k_mutex_unlock(&m_lock);

	shell_print(shell, "command succeeded");
	return 0;
}

static int cmd_reset_show(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	uint16_t count = m_hdr.reason_count;

	if (count == 0) {
		shell_print(shell, "<empty>");
		return 0;
	}

	for (uint16_t i = 0; i < count; i++) {
		struct ctr_trace_boot_rec rec;
		uint16_t n = 0;
		int ret = reason_get(m_dev, m_layout.reasons_off, m_layout.reason_cap,
				     m_hdr.reason_head, count - i, &rec, 1, &n);

		if (ret || n == 0) {
			continue;
		}

		shell_fprintf(shell, SHELL_NORMAL, "boot %u: cause ", rec.boot_num);
		shell_print_reset_cause(shell, rec.reset_cause);
		shell_fprintf(shell, SHELL_NORMAL, "\n");
	}

	return 0;
}

static int cmd_reset_clear(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!m_ready) {
		shell_error(shell, "not initialized");
		return -EAGAIN;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	m_hdr.reason_head = 0;
	m_hdr.reason_count = 0;
	store_header();
	k_mutex_unlock(&m_lock);

	shell_print(shell, "command succeeded");
	return 0;
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
	sub_trace_log,

	SHELL_CMD_ARG(show, NULL, "Show the primary log ring buffer.", cmd_log_show, 1, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear the primary log ring buffer.", cmd_log_clear, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_trace_fault,

	SHELL_CMD_ARG(show, NULL, "Show the pre-fault log snapshot.", cmd_fault_show, 1, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear the pre-fault log snapshot.", cmd_fault_clear, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_trace_reset,

	SHELL_CMD_ARG(show, NULL, "Show the boot/reset records.", cmd_reset_show, 1, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear the boot/reset records.", cmd_reset_clear, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_trace,

	SHELL_CMD_ARG(log, &sub_trace_log, "Primary log ring buffer commands.", print_help, 1, 0),
	SHELL_CMD_ARG(fault, &sub_trace_fault, "Pre-fault log snapshot commands.", print_help, 1, 0),
	SHELL_CMD_ARG(reset, &sub_trace_reset, "Boot/reset record commands.", print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(trace, &sub_trace, "Trace (post-mortem debug) commands.", print_help);

/* clang-format on */

#endif /* defined(CONFIG_CTR_TRACE_SHELL) */
