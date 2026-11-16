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

/* Third-party includes */
#include <zcbor_encode.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Initialize the cloud subsystem and start connecting in the background.
 *
 * Stores @p options (not copied, the structure must stay valid), starts the cloud
 * work queue and returns without waiting for the cloud: the work queue creates the
 * session and uploads the decoder, the encoder and the configuration, retrying all
 * but the configuration until they succeed. Use ctr_cloud_wait_initialized() to wait
 * for it. Call once; a call that failed leaves the subsystem unused and can be
 * repeated.
 *
 * @param options Hashes and optional decoder and encoder of the application.
 *
 * @retval 0         Started.
 * @retval -EINVAL   @p options is NULL, or the claim token is not 32 hex digits.
 * @retval -EPERM    The claim token is not set.
 * @retval -EALREADY Already started by a previous successful call.
 * @retval <0        Another error reading the device information or initializing
 *                   the transfer layer.
 */
int ctr_cloud_init(struct ctr_cloud_options *options);

/**
 * @brief Wait until the first cloud session has been created.
 *
 * @param timeout Time to wait.
 *
 * @retval 0          The session has been created (ctr_cloud_init() has finished
 *                    in the background).
 * @retval -ETIMEDOUT Not within @p timeout.
 */
int ctr_cloud_wait_initialized(k_timeout_t timeout);

/**
 * @brief Check whether the first cloud session has been created.
 *
 * @param[out] initialized Set to true once ctr_cloud_wait_initialized() would return 0.
 *
 * @retval 0       Success.
 * @retval -EINVAL @p initialized is NULL.
 */
int ctr_cloud_is_initialized(bool *initialized);

/**
 * @brief Set the callback for data received from the cloud.
 *
 * The callback gets CTR_CLOUD_EVENT_RECV with the payload of each downlink data
 * message. It runs in the thread that calls ctr_cloud_downlink() (the cloud work
 * queue when polling) with the cloud lock held. Waits for the cloud lock.
 *
 * @param user_cb   Callback, NULL to remove it.
 * @param user_data Passed to @p user_cb.
 *
 * @retval 0 Success.
 */
int ctr_cloud_set_callback(ctr_cloud_cb user_cb, void *user_data);

/**
 * @brief Set the period of polling the cloud for downlink messages.
 *
 * Takes effect immediately once the first session has been created, otherwise
 * when it is. The cloud is polled by ctr_cloud_downlink() on the cloud work queue.
 *
 * @param interval Poll period, K_NO_WAIT (the default) for no periodic polling.
 *
 * @retval 0 Success.
 */
int ctr_cloud_set_poll_interval(k_timeout_t interval);

/**
 * @brief Poll the cloud for downlink messages now.
 *
 * Schedules ctr_cloud_downlink() on the cloud work queue and returns.
 *
 * @retval 0      Success.
 * @retval -EPERM ctr_cloud_init() has not completed.
 * @retval <0     The work could not be scheduled.
 */
int ctr_cloud_poll_immediately(void);

/**
 * @brief Send data to the cloud with timeout.
 *
 * Copies @p buf (the encoded payload) into the cloud transfer buffer behind the
 * data packet header and sends it. Call only after ctr_cloud_init() has succeeded.
 *
 * @p timeout is one deadline for waiting for the cloud lock and for the transfer;
 * the transfer gets the time left and repeats an exchange whose response got lost
 * (after 1 s) or for which the network granted no connection (after 30 s) until the
 * deadline; K_FOREVER repeats without end. At the deadline the LTE layer gives the
 * exchange up once an AT dialog on the transfer buffers has ended, so the call can
 * return up to about 36 s past @p timeout.
 *
 * With CONFIG_CTR_CLOUD_SPOOL, a message that is not sent because the transfer fails
 * (or, for ctr_cloud_send_data_cb(), because the deadline expired after encoding)
 * stays in the spool and goes out with a later successful send; the return code
 * does not tell whether the message was queued, so a retry by the caller may
 * duplicate it.
 *
 * @param buf     Pointer to data buffer.
 * @param len     Length of data to send.
 * @param timeout Deadline for the lock wait and the transfer.
 *
 * @retval 0          Sent.
 * @retval -EINVAL    @p buf is NULL or @p len is zero.
 * @retval -EPERM     ctr_cloud_init() has not completed.
 * @retval -EBUSY     @p timeout is K_NO_WAIT and the cloud lock is taken.
 * @retval -EAGAIN    The deadline expired while waiting for the cloud lock.
 * @retval -EDEADLK   Called from inside an encoder callback.
 * @retval -ENOSPC    The data do not fit the transfer buffer.
 * @retval -ETIMEDOUT The transfer timed out.
 * @retval <0         Another transfer error.
 */
int ctr_cloud_send_data(const void *buf, size_t len, k_timeout_t timeout);

/**
 * @brief Send data to the cloud (blocking, no timeout).
 *
 * Equivalent to ctr_cloud_send_data() with K_FOREVER timeout, including the spool.
 *
 * @param buf  Pointer to data buffer.
 * @param len  Length of data to send.
 *
 * @retval 0        Sent.
 * @retval -EINVAL  @p buf is NULL or @p len is zero.
 * @retval -EPERM   ctr_cloud_init() has not completed.
 * @retval -EDEADLK Called from inside an encoder callback.
 * @retval -ENOSPC  The data do not fit the transfer buffer.
 * @retval <0       Another transfer error.
 */
int ctr_cloud_send(const void *buf, size_t len);

/**
 * @brief Callback encoding the CBOR payload of a data message.
 *
 * Encodes the payload of a data packet (UL_UPLOAD_DATA) with the zcbor encoding
 * state @p zs, which writes directly into the transfer buffer of the cloud subsystem.
 * The packet header is already in place and cannot be overwritten, and the callback
 * cannot write behind the end of the buffer: when the message does not fit, zcbor
 * reports ZCBOR_ERR_NO_PAYLOAD and the message is not sent.
 *
 * The callback must close every container (map, list) it opens.
 *
 * The state has stop_on_error enabled, so the callback does not need to check the
 * result of every put call. The first error is kept and nothing is written after it.
 *
 * The callback runs in the caller's thread with the cloud lock held. Calling
 * ctr_cloud_send_data(), ctr_cloud_send(), ctr_cloud_send_data_cb(),
 * ctr_cloud_downlink() or ctr_cloud_firmware_update() from it fails with -EDEADLK;
 * other blocking cloud calls (e.g. ctr_cloud_wait_initialized()) may deadlock.
 * Do not take a lock whose holder may call the cloud API.
 *
 * It may be called at most once per send, but a send is not guaranteed to call it
 * exactly once, so side effects (for example sequence numbers) should be kept
 * outside of the callback or be idempotent. The message must fit into the transfer
 * buffer minus the 9 byte packet header. The helpers ctr_cloud_cbor_free(),
 * ctr_cloud_cbor_len(), ctr_cloud_cbor_put_raw() and ctr_cloud_cbor_put_key_ts() are
 * meant to be used from the callback.
 *
 * Return 0 on success or a negative errno. Avoid -EAGAIN, -ETIMEDOUT, -EDEADLK
 * and -EPERM: the value is passed through unchanged and cannot be told apart
 * from the errors of ctr_cloud_send_data_cb() itself.
 *
 * @param zs        Encoding state positioned behind the packet header.
 * @param user_data User data passed by the caller of the send function.
 *
 * @retval 0        Success, the message is complete.
 * @retval negative Error (negative errno) which aborts the send and is returned to the
 *                  caller of the send function unchanged. Nothing is sent.
 * @retval positive Not allowed, reserved for the cloud subsystem; treated as -EINVAL.
 */
typedef int (*ctr_cloud_data_encode_cb)(zcbor_state_t *zs, void *user_data);

/**
 * @brief Encode a data message with a callback and send it to the cloud.
 *
 * Waits for the cloud lock, then lets @p cb encode the CBOR payload directly
 * into the cloud transfer buffer behind the data packet header, so the caller
 * needs no message buffer of its own. The callback is called only once the
 * transfer buffer is free, so the data reflect the moment of sending.
 *
 * @p timeout is one deadline for waiting for the cloud lock and for the
 * transfer: the time left after the lock is acquired and the payload is
 * encoded is the deadline of the transfer, which repeats failed exchanges
 * until it as ctr_cloud_send_data() does. If the deadline expires while
 * waiting for the lock, the callback is not called (-EAGAIN); if it has
 * expired before the transfer can start, nothing is sent (-ETIMEDOUT);
 * K_NO_WAIT therefore never sends.
 *
 * If encoding fails, nothing is sent. Test the result with ret != 0;
 * positive values are reserved.
 *
 * With CONFIG_CTR_CLOUD_SPOOL, a message that is not sent because the transfer fails
 * (or, for ctr_cloud_send_data_cb(), because the deadline expired after encoding)
 * stays in the spool and goes out with a later successful send; the return code
 * does not tell whether the message was queued, so a retry by the caller may
 * duplicate it.
 *
 * @param cb        Encoder callback, see ctr_cloud_data_encode_cb.
 * @param user_data Passed to @p cb.
 * @param timeout   Deadline for the lock wait; the rest is the transfer timeout.
 *
 * @retval 0          Sent.
 * @retval -EINVAL    @p cb is NULL, the callback returned a positive value or
 *                    encoded an empty payload.
 * @retval -EPERM     ctr_cloud_init() has not completed.
 * @retval -EAGAIN    The deadline expired while waiting for the cloud lock;
 *                    the callback was not called.
 * @retval -ETIMEDOUT The deadline expired before the transfer could start, or
 *                    the transfer timed out.
 * @retval -EDEADLK   Called from inside an encoder callback.
 * @retval -ENOSPC    The payload does not fit the transfer buffer.
 * @retval -ENOMEM    Not enough zcbor backup states (raise
 *                    CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES).
 * @retval -EBADMSG   The payload is not valid CBOR.
 * @retval <0         A negative error returned by @p cb, or another transfer
 *                    error.
 */
int ctr_cloud_send_data_cb(ctr_cloud_data_encode_cb cb, void *user_data, k_timeout_t timeout);

/**
 * @brief Get the number of free bytes left for the payload.
 *
 * Valid only for the state passed to the encode callback.
 *
 * @param zs Encoding state passed to the callback.
 *
 * @return Number of bytes that can still be written.
 */
size_t ctr_cloud_cbor_free(const zcbor_state_t *zs);

/**
 * @brief Get the number of payload bytes encoded so far.
 *
 * Valid only for the state passed to the encode callback; the packet header
 * is not counted. Returns 0 outside of an encode callback or for a foreign state.
 *
 * @param zs Encoding state passed to the callback.
 *
 * @return Number of payload bytes written so far.
 */
size_t ctr_cloud_cbor_len(const zcbor_state_t *zs);

/**
 * @brief Insert an already encoded CBOR value into the payload.
 *
 * Writes no key. @p cbor must be exactly one complete CBOR data item (a whole map,
 * list, ...); this is not validated. The item is counted in the element count of
 * @p zs like any other encoded element.
 *
 * On lack of space the zcbor state is put into the ZCBOR_ERR_NO_PAYLOAD error,
 * so the failure cannot be missed even if the return value is ignored (the send then
 * fails with -ENOSPC).
 *
 * @param zs   Encoding state passed to the callback.
 * @param cbor Encoded CBOR value.
 * @param len  Length of the encoded value in bytes.
 *
 * @retval 0       Success.
 * @retval -EINVAL  @p zs is NULL, or @p cbor is NULL or @p len is zero. In the latter
 *                  case nothing is written, but the state is put into the
 *                  ZCBOR_ERR_WRONG_VALUE error, so the failure cannot be missed.
 * @retval -ENOSPC  Not enough space (the state is put into the ZCBOR_ERR_NO_PAYLOAD
 *                  error), or the state already holds that error. Nothing is written.
 * @retval -EBADMSG The state already holds any other error. Nothing is written.
 */
int ctr_cloud_cbor_put_raw(zcbor_state_t *zs, const void *cbor, size_t len);

/**
 * @brief Encode a key and a timestamp.
 *
 * Writes @p key followed by @p ts as an integer, or by null when @p ts is zero
 * (RTC not set). Errors are reported in the zcbor state.
 *
 * @param zs  Encoding state passed to the callback.
 * @param key Map key.
 * @param ts  Unix timestamp in seconds, 0 for none.
 */
void ctr_cloud_cbor_put_key_ts(zcbor_state_t *zs, uint32_t key, int64_t ts);

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

/**
 * @brief Request a firmware update from the cloud.
 *
 * Sends a request to download the application firmware @p firmwareId; the firmware
 * then arrives in downlink messages. Waits for the cloud lock and the transfer
 * without a timeout.
 *
 * @param firmwareId Firmware identifier, 1 to 200 characters (not NULL).
 *
 * @retval 0        The request has been sent.
 * @retval -EINVAL  @p firmwareId is empty or longer than 200 characters.
 * @retval -EPERM   ctr_cloud_init() has not completed.
 * @retval -EDEADLK Called from inside an encoder callback.
 * @retval <0       Packing or transfer error.
 */
int ctr_cloud_firmware_update(const char *firmwareId);

/**
 * @brief Download pending data from the cloud.
 *
 * Downloads one downlink message and processes it (session, time, configuration,
 * data for the callback of ctr_cloud_set_callback(), shell commands, firmware),
 * uploading a response when the message requires one. When the cloud has more
 * messages, another download is scheduled on the cloud work queue.
 *
 * @p timeout is one deadline for waiting for the cloud lock and for the download,
 * which repeats failed exchanges until it as ctr_cloud_send_data() does. The
 * timeout applies only to the initial download - any required response upload
 * uses K_FOREVER.
 *
 * @param timeout Deadline for the lock wait and the download.
 *
 * @retval 0          Success, also when there was nothing to download.
 * @retval -EPERM     ctr_cloud_init() has not completed.
 * @retval -EBUSY     @p timeout is K_NO_WAIT and the cloud lock is taken.
 * @retval -EAGAIN    The deadline expired while waiting for the cloud lock.
 * @retval -EDEADLK   Called from inside an encoder callback.
 * @retval -ETIMEDOUT The download timed out.
 * @retval <0         Another transfer error, or the message could not be processed.
 */
int ctr_cloud_downlink(k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CLOUD_H_ */
