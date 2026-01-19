/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LTE_V2_H_
#define CHESTER_INCLUDE_CTR_LTE_V2_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_lte_v2 ctr_lte_v2
 * @brief LTE modem control library
 *
 * @{
 */

/**
 * @brief LTE connection parameters (returned by @ref ctr_lte_v2_get_conn_param).
 *
 * If @ref valid is false, other fields are undefined.
 */
struct ctr_lte_v2_conn_param {
	bool valid; /**< True if values are valid. */
	int result; /**< Connection evaluation result (convert to text with @ref
		       ctr_lte_v2_str_coneval_result). */
	int eest;   /**< Energy estimate (vendor-specific). */
	int ecl;    /**< Coverage enhancement level (e.g. 0..2 for NB-IoT). */
	int rsrp;   /**< Reference Signal Received Power (dBm). */
	int rsrq;   /**< Reference Signal Received Quality (dB). */
	int snr;    /**< Signal-to-noise ratio. */
	int plmn;   /**< PLMN code (MCCMNC) as integer. */
	int cid;    /**< Cell ID. */
	int band;   /**< LTE band number. */
	int earfcn; /**< EARFCN (channel). */
};

/**
 * @brief Registration status values from +CEREG.
 */
enum ctr_lte_v2_cereg_param_stat {
	CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED = 0,      /**< Not registered, not searching. */
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME = 1,     /**< Registered in home network. */
	CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING = 2,           /**< Searching for network. */
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED = 3, /**< Registration denied. */
	CTR_LTE_V2_CEREG_PARAM_STAT_UNKNOWN = 4,             /**< Unknown status. */
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING = 5,  /**< Registered, roaming. */
	CTR_LTE_V2_CEREG_PARAM_STAT_SIM_FAILURE = 90,        /**< SIM failure. */
};

/**
 * @brief Access technology values (CEREG:AcT).
 */
enum ctr_lte_v2_cereg_param_act {
	CTR_LTE_V2_CEREG_PARAM_ACT_UNKNOWN = 0, /**< Unknown. */
	CTR_LTE_V2_CEREG_PARAM_ACT_LTE = 7,     /**< LTE. */
	CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT = 9,   /**< NB-IoT. */
};

#define CTR_LTE_V2_CEREG_PARAM_ACTIVE_TIME_DISABLED      -1
#define CTR_LTE_V2_CEREG_PARAM_PERIODIC_TAU_EXT_DISABLED -1

/**
 * @brief Attach policy for retry and timeout configuration.
 */
enum ctr_lte_v2_attach_policy {
	CTR_LTE_V2_ATTACH_POLICY_AGGRESSIVE = 0,   /**< Aggressive attach. */
	CTR_LTE_V2_ATTACH_POLICY_PERIODIC_2H = 1,  /**< Periodic attach every 2 hours. */
	CTR_LTE_V2_ATTACH_POLICY_PERIODIC_6H = 2,  /**< Periodic attach every 6 hours. */
	CTR_LTE_V2_ATTACH_POLICY_PERIODIC_12H = 3, /**< Periodic attach every 12 hours. */
	CTR_LTE_V2_ATTACH_POLICY_PERIODIC_1D = 4,  /**< Periodic attach every 1 day. */
	CTR_LTE_V2_ATTACH_POLICY_PROGRESSIVE = 5,  /**< Progressive attach. */
};

/**
 * @brief Decoded +CEREG parameters.
 */
struct ctr_lte_v2_cereg_param {
	bool valid;                            /**< True if values are valid. */
	enum ctr_lte_v2_cereg_param_stat stat; /**< Registration status. */
	char tac[5]; /**< Tracking Area Code (hex string, 4 chars + '\0'). */
	int cid;     /**< Cell Identity (E-UTRAN). */
	enum ctr_lte_v2_cereg_param_act act; /**< Access technology. */
	uint8_t cause_type;                  /**< Reject cause type. */
	uint8_t reject_cause;                /**< Reject cause code (3GPP). */
	int active_time;                     /**< Active-Time (seconds), -1 if disabled. */
	int periodic_tau_ext; /**< Extended periodic TAU (seconds), -1 if disabled. */
};

/**
 * @brief Parameters for sending and optionally receiving data.
 */
struct ctr_lte_v2_send_recv_param {
	bool rai;             /**< Use Release Assistance Indication if supported. */
	bool send_as_string;  /**< Data should be sent as a string. */
	const void *send_buf; /**< Buffer with data to send. */
	size_t send_len;      /**< Length of data to send. */
	void *recv_buf;       /**< Buffer for received data (NULL if not needed). */
	size_t recv_size;     /**< Size of receive buffer. */
	size_t *recv_len;     /**< Output: number of received bytes (can not be null if is set
			       * recv_buf).
			       */
	k_timeout_t timeout;  /**< Soft timeout for the operation. When timeout expires, FSM
			       * completes current operation gracefully before returning.
			       * Not a hard abort - signals "finish when you can".
			       */
};

/**
 * @brief Communication metrics and timing information.
 *
 * Timestamps *_last_ts are uptime values in milliseconds
 * (see @c ctr_rtc_get_ts()).
 */
struct ctr_lte_v2_metrics {
	uint32_t attach_count;            /**< Number of successful attach. */
	uint32_t attach_fail_count;       /**< Number of failed attach attempts. */
	uint32_t attach_duration_ms;      /**< Total attach duration (ms). */
	int64_t attach_last_ts;           /**< Uptime of last attach (ms). */
	uint32_t attach_last_duration_ms; /**< Duration of last attach (ms). */

	uint32_t uplink_count;  /**< Number of uplink transmissions. */
	uint32_t uplink_bytes;  /**< Total uplink bytes. */
	uint32_t uplink_errors; /**< Uplink error count. */
	int64_t uplink_last_ts; /**< Uptime of last uplink (ms). */

	uint32_t downlink_count;  /**< Number of downlink receptions. */
	uint32_t downlink_bytes;  /**< Total downlink bytes. */
	uint32_t downlink_errors; /**< Downlink error count. */
	int64_t downlink_last_ts; /**< Uptime of last downlink (ms). */

	uint32_t cscon_1_duration_ms;      /**< Total time in RRC Connected (CSCON=1). */
	uint32_t cscon_1_last_duration_ms; /**< Duration of last RRC Connected period. */
};

/**
 * @brief Enable LTE modem and data connection.
 *
 * Performs initialization, configuration, and network attach.
 *
 * @retval 0   Success.
 * @retval -ENOTSUP Test mode is enabled.
 */
int ctr_lte_v2_enable(void);

/**
 * @brief Disconnect and reconnect LTE modem.
 *
 * Useful for recovering from connection issues.
 *
 * @retval 0   Success.
 * @retval -ENOTSUP Test mode is enabled.
 * @retval -ENODEV Modem is disabled.
 */
int ctr_lte_v2_reconnect(void);

/**
 * @brief Wait for connection to be established.
 *
 * @param timeout  Maximum wait duration.
 * @retval 0       Connected.
 * @retval -ENOTSUP Test mode is enabled.
 * @retval -ETIMEDOUT Timeout expired.
 */
int ctr_lte_v2_wait_for_connected(k_timeout_t timeout);

/**
 * @brief Send data and optionally receive response.
 *
 * Sends data to the network and optionally waits for a response.
 *
 * Uses soft timeout: when timeout expires, FSM completes current operation
 * gracefully before returning. The timeout signals "finish when you can",
 * not an immediate abort. Return code reflects actual result of the operation.
 *
 * @param param  Send/receive parameters (see @ref ctr_lte_v2_send_recv_param).
 *
 * @retval 0          Success.
 * @retval -ETIMEDOUT Operation timed out (returned after FSM completed gracefully).
 * @retval -ECONNRESET Connection was reset (CSCON=0 received during send).
 * @retval -ENETDOWN  Network is down (deregistered from network).
 * @retval -EIO       I/O error (send/receive failed or general error).
 * @retval -EFAULT    FSM stuck (internal error, STOP_BIT not received in 30s).
 */
int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param);

/* -------- Information getters -------- */

// get information
int ctr_lte_v2_get_imei(uint64_t *imei);
int ctr_lte_v2_get_imsi(uint64_t *imsi);
int ctr_lte_v2_get_iccid(char **iccid);
int ctr_lte_v2_get_modem_fw_version(char **version);
int ctr_lte_v2_get_conn_param(struct ctr_lte_v2_conn_param *param);
int ctr_lte_v2_get_cereg_param(struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_get_metrics(struct ctr_lte_v2_metrics *metrics);
int ctr_lte_v2_is_attached(bool *attached);

/**
 * @brief Get current attach information.
 *
 * @note This API is experimental and may change in future releases.
 *
 * @param attempt         Input: current attach attempt (0 for first).
 * @param attach_timeout_sec Output: current attach timeout in seconds.
 * @param retry_delay_sec    Output: current retry delay in seconds.
 * @param remaining_sec      Output: remaining time for current timeout in seconds.
 * @retval 0               Success.
 */
int ctr_lte_v2_get_curr_attach_info(int *attempt, int *attach_timeout_sec, int *retry_delay_sec,
				    int *remaining_sec);

/* -------- GNSS functions -------- */

#if defined(CONFIG_CTR_LTE_V2_GNSS)

struct ctr_lte_v2_gnss_update {
	float latitude;
	float longitude;
	float altitude;
	float accuracy;
	float speed;
	float heading;
	char datetime[20];
};
typedef void (*ctr_lte_v2_gnss_cb)(const struct ctr_lte_v2_gnss_update *update, void *user_data);
int ctr_lte_v2_gnss_set_enable(bool enable);
int ctr_lte_v2_gnss_get_enable(bool *enable);
int ctr_lte_v2_gnss_set_handler(ctr_lte_v2_gnss_cb callback, void *user_data);
#endif

/* -------- Utility functions -------- */

/** Convert connection evaluation result code to string. */
const char *ctr_lte_v2_str_coneval_result(int result);
/** Convert +CEREG registration status to text. */
const char *ctr_lte_v2_str_cereg_stat(enum ctr_lte_v2_cereg_param_stat stat);
/** Convert +CEREG registration status to human-readable text. */
const char *ctr_lte_v2_str_cereg_stat_human(enum ctr_lte_v2_cereg_param_stat stat);
/** Convert access technology (AcT) to text. */
const char *ctr_lte_v2_str_act(enum ctr_lte_v2_cereg_param_act act);

/** @} */ /* end of group hio_lte_modem */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LTE_V2_H_ */
