/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_CLOUD_H_
#define CHESTER_INCLUDE_CTR_CLOUD_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CTR_CLOUD_TRANSFER_BUF_SIZE (16 * 1024)

/**
 * @brief Cloud communication metrics.
 *
 * Timestamps *_last_ts are Unix timestamps in seconds
 * (see @c ctr_rtc_get_ts()). Value -1 indicates that the
 * event has never occurred.
 */
struct ctr_cloud_metrics {
	int64_t timestamp; /**< Timestamp when metrics were retrieved (sec). */

	uint32_t uplink_count;     /**< Number of uplink messages. */
	uint32_t uplink_bytes;     /**< Total uplink bytes. */
	uint32_t uplink_fragments; /**< Number of uplink fragments. */
	int64_t uplink_last_ts;    /**< Timestamp of last successful uplink (sec). */

	uint32_t uplink_errors;       /**< Uplink error count. */
	int64_t uplink_error_last_ts; /**< Timestamp of last uplink error (sec). */

	uint32_t downlink_count;     /**< Number of downlink messages. */
	uint32_t downlink_fragments; /**< Number of downlink fragments. */
	uint32_t downlink_bytes;     /**< Total downlink bytes. */
	int64_t downlink_last_ts;    /**< Timestamp of last successful downlink (sec). */

	uint32_t downlink_errors;       /**< Downlink error count. */
	int64_t downlink_error_last_ts; /**< Timestamp of last downlink error (sec). */

	uint32_t poll_count;  /**< Number of poll operations. */
	int64_t poll_last_ts; /**< Timestamp of last poll (sec). */

	uint32_t uplink_data_count;  /**< Number of data messages sent. */
	int64_t uplink_data_last_ts; /**< Timestamp of last data message sent (sec). */

	uint32_t downlink_data_count;  /**< Number of data messages received. */
	int64_t downlink_data_last_ts; /**< Timestamp of last data message received (sec). */

	uint32_t recv_shell_count;  /**< Number of shell commands received. */
	int64_t recv_shell_last_ts; /**< Timestamp of last shell command received (sec). */
};

struct ctr_cloud_options {
	uint64_t decoder_hash;
	uint64_t encoder_hash;
	const uint8_t *decoder_buf;
	size_t decoder_len;
	const uint8_t *encoder_buf;
	size_t encoder_len;
};

struct ctr_cloud_session {
	uint32_t id;
	uint64_t decoder_hash;
	uint64_t encoder_hash;
	uint64_t config_hash;
	int64_t timestamp;
	char device_id[36 + 1];
	char device_name[32 + 1];
};

enum ctr_cloud_event {
	CTR_CLOUD_EVENT_CONNECTED,
	CTR_CLOUD_EVENT_RECV,
};

struct ctr_cloud_event_data_recv {
	void *buf;
	size_t len;
};

union ctr_cloud_event_data {
	struct ctr_cloud_event_data_recv recv;
};

typedef void (*ctr_cloud_cb)(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param);

int ctr_cloud_init(struct ctr_cloud_options *options);
int ctr_cloud_wait_initialized(k_timeout_t timeout);
int ctr_cloud_is_initialized(bool *initialized);
int ctr_cloud_set_callback(ctr_cloud_cb user_cb, void *user_data);
int ctr_cloud_set_poll_interval(k_timeout_t interval);
int ctr_cloud_poll_immediately(void);

/**
 * @brief Send data to the cloud with timeout.
 *
 * Uses soft timeout: when timeout expires, the underlying LTE operation
 * completes gracefully before returning. The timeout signals "finish when
 * you can", not an immediate abort.
 *
 * @param buf     Pointer to data buffer.
 * @param len     Length of data to send.
 * @param timeout Soft timeout for the operation.
 *
 * @retval 0          Success.
 * @retval -EINVAL    Invalid argument (NULL buffer or zero length).
 * @retval -ETIMEDOUT Operation timed out.
 * @retval -EIO       I/O error.
 */
int ctr_cloud_send_data(const void *buf, size_t len, k_timeout_t timeout);

/**
 * @brief Send data to the cloud (blocking, no timeout).
 *
 * Equivalent to ctr_cloud_send_data() with K_FOREVER timeout.
 *
 * @param buf  Pointer to data buffer.
 * @param len  Length of data to send.
 *
 * @retval 0       Success.
 * @retval -EINVAL Invalid argument.
 * @retval -EIO    I/O error.
 */
int ctr_cloud_send(const void *buf, size_t len);

/**
 * @brief Get timestamp of last successful cloud communication.
 *
 * Returns the most recent timestamp from uplink, downlink, or poll operations.
 *
 * @param[out] ts Pointer to store the timestamp (Unix timestamp in sec).
 *                Returns 0 if no communication has occurred yet.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument (NULL pointer).
 * @retval -EPERM Cloud not initialized or session not established.
 */
int ctr_cloud_get_last_seen_ts(int64_t *ts);

/**
 * @brief Get cloud communication metrics.
 *
 * @param[out] metrics Pointer to structure to fill with metrics data.
 *
 * @retval 0 Success.
 * @retval -EINVAL Invalid argument (NULL pointer).
 */
int ctr_cloud_get_metrics(struct ctr_cloud_metrics *metrics);

int ctr_cloud_firmware_update(const char *firmwareId);

/**
 * @brief Download pending data from the cloud.
 *
 * Uses soft timeout: when timeout expires, the underlying LTE operation
 * completes gracefully before returning. The timeout applies only to the
 * initial download - any required response upload uses K_FOREVER.
 *
 * @param timeout Soft timeout for the download operation.
 *
 * @retval 0          Success.
 * @retval -ETIMEDOUT Operation timed out.
 * @retval -EIO       I/O error.
 */
int ctr_cloud_downlink(k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CLOUD_H_ */
