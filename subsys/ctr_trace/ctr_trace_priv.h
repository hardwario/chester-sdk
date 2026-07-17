/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Internal, non-public definitions of the ctr_trace subsystem: the on-retained-
 * memory data types and layout constants shared between ctr_trace.c and the
 * Twister suite under tests/subsys/ctr_trace. The layout/ring/reason helpers are
 * static in ctr_trace.c; the tests reach them by compiling that source directly
 * (via #include "ctr_trace.c").
 */

#ifndef CHESTER_SUBSYS_CTR_TRACE_PRIV_H_
#define CHESTER_SUBSYS_CTR_TRACE_PRIV_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Magic marker ("TRCE") identifying a valid retained header. */
#define CTR_TRACE_MAGIC        0x54524345U
/** Retained-layout version; bump on any incompatible header/layout change. */
#define CTR_TRACE_VERSION      1U
/** Minimum size (bytes) each of the two log buffers must have to be usable. */
#define CTR_TRACE_MIN_LOG_SIZE 128U

/** One boot/reset record stored in the retained reset-reason ring. */
struct ctr_trace_boot_rec {
	/** Monotonic boot counter value for this boot (1-based). */
	uint16_t boot_num;
	uint16_t _reserved;
	/** Reset cause bitmask as reported by `hwinfo_get_reset_cause()`. */
	uint32_t reset_cause;
};

/**
 * Header stored at offset 0 of the retained region. Written as a whole blob;
 * `fault_marker` is additionally poked in place from the fatal-error handler,
 * so its offset must stay stable.
 */
struct ctr_trace_header {
	uint32_t magic;
	uint16_t version;
	uint16_t boot_count;

	uint8_t fault_marker;    /* set by k_sys_fatal_error_handler, cleared at boot */
	uint8_t secondary_valid; /* secondary buffer holds a valid fault snapshot */
	uint16_t reason_head;    /* next reset-record slot to write */
	uint16_t reason_count;   /* valid reset records (<= capacity) */
	uint16_t _pad;

	uint32_t primary_head;  /* next write index into the primary ring */
	uint32_t primary_count; /* valid bytes in the primary ring (<= log_size) */
	uint32_t secondary_len; /* valid bytes in the secondary (linear) buffer */
};

/** Computed partitioning of the retained region. */
struct ctr_trace_layout {
	off_t reasons_off;   /* start of the reset-record ring */
	uint16_t reason_cap; /* number of reset records that fit */
	off_t primary_off;   /* start of the primary log ring */
	off_t secondary_off; /* start of the secondary log buffer */
	size_t log_size;     /* size of each log buffer (both equal) */
};

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_TRACE_PRIV_H_ */
