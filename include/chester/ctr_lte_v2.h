/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LTE_V2_H_
#define CHESTER_INCLUDE_CTR_LTE_V2_H_

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>

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
	k_timeout_t timeout;  /**< Deadline for the whole exchange, including the wait for
			       * another one in progress (see @ref ctr_lte_v2_send_recv).
			       */
	int retry_count;      /**< Number of extra sends of the data when the network grants
			       * no connection for them. Each send waits for the connection
			       * for the time left to the deadline, at least 5 s and at most
			       * 120 s. 0 (the default) sends once; a negative value counts
			       * as 0. The call returns -ENOTCONN for a missing connection
			       * only when the last send has more than 120 s left (or no
			       * deadline); with less, the deadline comes first and the call
			       * returns -ETIMEDOUT.
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
 * Useful for recovering from connection issues. The call only requests the reconnect: the LTE
 * thread carries it out once it is done with its current step (an AT dialog in progress ends
 * first).
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
 * The LTE stack works on its own copy of @p param. When the timeout expires before the
 * exchange has ended, the call gives the exchange up and returns -ETIMEDOUT; the stack goes on
 * with what it is doing (for example the network attach). If the modem is just then exchanging
 * the data with the caller's buffers, the call first waits for that AT dialog to end, which
 * can take up to about 36 s past the timeout; a response received in that wait is dropped.
 * The result follows who ended the exchange, not the time: an exchange the stack has completed
 * returns 0 even when that happened just past the timeout.
 *
 * The buffers in @p param are not touched after the call returns.
 *
 * After -ENOTCONN for a missing connection, 'lte state' shows the extended error report of the
 * modem (AT+CEER).
 *
 * @param param  Send/receive parameters (see @ref ctr_lte_v2_send_recv_param).
 *
 * @retval 0          Success.
 * @retval -ETIMEDOUT The exchange did not end before the timeout, or the response did not come.
 * @retval -ENOTCONN  LTE is disabled, the socket is not usable, or the network granted no
 *                    connection for the data (see @ref ctr_lte_v2_send_recv_param.retry_count).
 * @retval -EBUSY     Another exchange kept the stack busy until the timeout.
 * @retval -ENOTSUP   Test mode is enabled.
 * @retval -EINVAL    @p param is NULL, or has recv_buf without recv_size or recv_len.
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

/**
 * @brief One registration status change (CEREG).
 */
struct ctr_lte_v2_cereg_event {
	uint32_t uptime_s;    /**< Uptime of the change. */
	uint32_t cid;         /**< Cell ID, 0 if not reported. */
	uint32_t plmn;        /**< PLMN (MCCMNC) of the last %CONEVAL if on this cell, else 0. */
	uint16_t tac;         /**< Tracking area code, 0 if not reported. */
	uint8_t stat;         /**< @ref ctr_lte_v2_cereg_param_stat. */
	uint8_t act;          /**< @ref ctr_lte_v2_cereg_param_act. */
	uint8_t reject_cause; /**< EMM cause, 0 if none (also set on an accepted registration). */
};

/**
 * @brief Get the last registration status changes, newest first.
 *
 * Keeps CONFIG_CTR_LTE_V2_CEREG_HISTORY changes; a repeat of the newest one is not stored.
 *
 * @param events Output array.
 * @param max    Size of @p events.
 * @param count  Number of events written.
 *
 * @retval 0       Success.
 * @retval -EINVAL Invalid argument.
 */
int ctr_lte_v2_get_cereg_history(struct ctr_lte_v2_cereg_event *events, size_t max, size_t *count);

/**
 * @brief Get the last registration reject (CEREG with an EMM cause while not registered).
 *
 * Kept separately, so it survives the history rolling over.
 *
 * @retval 0        Success.
 * @retval -EINVAL  Invalid argument.
 * @retval -ENODATA No reject since boot.
 */
int ctr_lte_v2_get_last_reject(struct ctr_lte_v2_cereg_event *event);

/* -------- Callbacks for LTE events -------- */

/**
 * @brief LTE events reported to the callbacks.
 */
enum ctr_lte_v2_cb_event {
	CTR_LTE_V2_CB_EVENT_CSCON_0 = 0, /**< Connection Status (Idle). */
	CTR_LTE_V2_CB_EVENT_CSCON_1 = 1, /**< Connection Status (Connected). */
	/** The neighbour cell measurement (ctr_lte_v2_schedule_ncellmeas) has ended. */
	CTR_LTE_V2_CB_EVENT_NCELLMEAS_DONE = 2,
	/** The network scan (ctr_lte_v2_scan) has ended, before the modem is restored. */
	CTR_LTE_V2_CB_EVENT_SCAN_DONE = 3,
};

/**
 * @brief LTE event callback.
 *
 * @note The handler runs on the ctr_lte_v2 work queue; it must not block and must return
 *       quickly. It may remove its own callback, but no other one.
 */
struct ctr_lte_v2_cb {
	sys_snode_t node; /**< Zephyr singly-linked list support. */
	void (*handler)(struct ctr_lte_v2_cb *cb, enum ctr_lte_v2_cb_event event);
	void *user_data; /**< User data owned by the registrant. */
};

/**
 * @brief Add an LTE event callback.
 *
 * @retval 0         Success.
 * @retval -EALREADY Callback already registered.
 * @retval -EINVAL   Invalid argument.
 */
int ctr_lte_v2_add_callback(struct ctr_lte_v2_cb *cb);

/**
 * @brief Remove an LTE event callback.
 *
 * @retval 0       Success.
 * @retval -ENOENT Callback not found.
 * @retval -EINVAL Invalid argument.
 */
int ctr_lte_v2_remove_callback(struct ctr_lte_v2_cb *cb);

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

/* -------- Neighbour cell measurement and network scan -------- */

#if defined(CONFIG_CTR_LTE_V2_SCAN)

/* %NCELLMEAS=5,7 (GCI search). rsrp and rsrq are the indexes the modem reports: RSRP in dBm is
 * rsrp - 141 (RSRP_IDX_TO_DBM of nrf for an index from 0, as the shell prints it; %CONEVAL in
 * ctr_lte_v2_conn_param takes - 140), RSRQ in dB is (rsrq - 39) / 2 with integer division (as
 * for %CONEVAL) */
#define CTR_LTE_V2_NCELLMEAS_CELL_MAX  7
#define CTR_LTE_V2_NCELLMEAS_NCELL_MAX 10

struct ctr_lte_v2_ncellmeas_ncell {
	uint32_t earfcn;
	uint16_t pci;
	int16_t rsrp;  /* index, see above */
	int16_t rsrq;  /* index, see above */
	int time_diff; /* ms between the measurement of its cell and this one, 0 if not valid */
};

struct ctr_lte_v2_ncellmeas_cell {
	uint32_t eci;
	uint32_t earfcn;
	char plmn[7];
	uint16_t tac;
	uint16_t adv; /* timing advance as reported, 65535 if not valid */
	uint16_t pci;
	int16_t rsrp; /* index, see above */
	int16_t rsrq; /* index, see above */
	bool serving;
	uint8_t neighbor_count; /* stored neighbours, at ncells[ncell_first] */
	uint8_t ncell_first;
};

struct ctr_lte_v2_ncellmeas_param {
	/* True after a readable URC: status and the cells are from it, otherwise no cells */
	bool valid;
	/* <status> of the URC if valid: 0 measured (at least the serving cell), 1 failed (no
	 * cells); 3 is taken with the cells it has, another status is not read (valid false) */
	uint8_t status;
	int result;        /* how it ended, see ctr_lte_v2_get_ncellmeas_param() */
	uint32_t uptime_s; /* k_uptime_seconds() at the end */
	uint8_t num_cells;
	uint8_t num_ncells;
	struct ctr_lte_v2_ncellmeas_cell cells[CTR_LTE_V2_NCELLMEAS_CELL_MAX];
	struct ctr_lte_v2_ncellmeas_ncell ncells[CTR_LTE_V2_NCELLMEAS_NCELL_MAX];
};

/**
 * @brief Request a neighbour cell measurement; it runs once the modem is registered and the RRC
 *        connection is idle, a sleeping modem is woken up for it.
 *
 * The callbacks of CTR_LTE_V2_CB_EVENT_NCELLMEAS_DONE run before the end is published: a request
 * from one gets -EALREADY and no measurement follows.
 *
 * @retval 0         Requested.
 * @retval -EALREADY Requested or running already (the request is posted again).
 * @retval -EBUSY    A scan is requested or running.
 * @retval -ENODEV   LTE is disabled.
 * @retval -ENOTSUP  Test mode.
 */
int ctr_lte_v2_schedule_ncellmeas(void);

/**
 * @brief Wait for the end of the requested measurement.
 *
 * A request dropped (LTE disabled, also by ctr_lte_v2_reconnect()) returns 0 as an end does, the
 * last measurement is then an older one: only an end calls CTR_LTE_V2_CB_EVENT_NCELLMEAS_DONE. Do
 * not wait from that callback, it runs before the end is published.
 *
 * @retval 0          Ended, or dropped (LTE disabled), or nothing was requested.
 * @retval -ETIMEDOUT Still requested or running.
 */
int ctr_lte_v2_wait_for_ncellmeas(k_timeout_t timeout);

/**
 * @brief Get the last measurement: result 0, -ENODATA (none yet), -ETIMEDOUT (cells after
 *        %NCELLMEASSTOP, if any), -EIO (rejected), -EBADMSG (unreadable URC), -ECANCELED.
 *
 * A scan with the cells (CTR_LTE_V2_SCAN_ALL, CTR_LTE_V2_SCAN_CELLS) overwrites it, without the
 * callback: -ENODATA while it measures a RAT, then the cells of the last RAT it measured.
 */
int ctr_lte_v2_get_ncellmeas_param(struct ctr_lte_v2_ncellmeas_param *param);

/* A network of the list from AT+COPS=? */
#define CTR_LTE_V2_SCAN_NETWORK_MAX 10

struct ctr_lte_v2_scan_network {
	char plmn[7];
	uint8_t stat; /* 0 unknown, 1 available, 2 current, 3 forbidden */
	uint8_t act;  /* 7 LTE-M, 9 NB-IoT */
};

/* The cells of the scan, from %NCELLMEAS in each RAT */
#define CTR_LTE_V2_SCAN_CELL_MAX 14

struct ctr_lte_v2_scan_cell {
	uint32_t eci;
	uint32_t earfcn;
	char plmn[7];
	uint16_t tac;
	uint16_t pci;
	int16_t rsrp; /* index as in ctr_lte_v2_ncellmeas_cell, in dBm rsrp - 141 */
	uint8_t act;  /* the RAT of the measurement: 7 LTE-M, 9 NB-IoT */
	bool serving;
};

/* A step: 0 ok, -ENODATA not run, -ETIMEDOUT, -EIO rejected, -EBADMSG unreadable, -ECANCELED */
struct ctr_lte_v2_scan_rat {
	int registration;
	int cells; /* -ETIMEDOUT: stopped at its bound, with the cells found so far */
	int networks;
};

/* What the scan covers in each RAT, after the reset and the registration */
enum ctr_lte_v2_scan_mode {
	CTR_LTE_V2_SCAN_ALL = 0, /* the cells (%NCELLMEAS), then the networks (AT+COPS=?) */
	CTR_LTE_V2_SCAN_PLMN,    /* the networks only */
	CTR_LTE_V2_SCAN_CELLS,   /* the cells only */
};

struct ctr_lte_v2_scan_result {
	bool valid;
	uint8_t mode;      /* enum ctr_lte_v2_scan_mode */
	int64_t uptime_ms; /* the start */
	uint32_t duration_s;
	struct ctr_lte_v2_scan_rat lte_m;
	struct ctr_lte_v2_scan_rat nb_iot;
	uint8_t cell_count;
	struct ctr_lte_v2_scan_cell cells[CTR_LTE_V2_SCAN_CELL_MAX];
	uint8_t network_count; /* the networks of both lists, may exceed the stored ones */
	struct ctr_lte_v2_scan_network networks[CTR_LTE_V2_SCAN_NETWORK_MAX];
};

/**
 * @brief Request a network scan: in each configured RAT (LTE-M first) a reset of the modem, the
 *        registration, then the cells and the network list as the mode says; then the modem is
 *        reset and attaches again. LTE is offline for minutes, a transaction meanwhile fails with
 *        -ENOTCONN. The step a mode skips stays -ENODATA.
 *
 * The callbacks of CTR_LTE_V2_CB_EVENT_SCAN_DONE run before the end is published: a request from
 * one gets -EALREADY and no scan follows.
 *
 * @retval 0         Requested.
 * @retval -EALREADY Requested or running already, with its own mode (the request is posted
 *                   again).
 * @retval -EINVAL   Invalid mode.
 * @retval -ENODEV   LTE is disabled.
 * @retval -ENOTSUP  Test mode.
 */
int ctr_lte_v2_scan(enum ctr_lte_v2_scan_mode mode);

/**
 * @brief Wait for the end of the requested scan.
 *
 * A request dropped (LTE disabled, also by ctr_lte_v2_reconnect()) returns 0 as an end does, the
 * result is then an older one: only an end calls CTR_LTE_V2_CB_EVENT_SCAN_DONE and changes
 * uptime_ms of the result. Do not wait from that callback, it runs before the end is published.
 *
 * @retval 0          Ended, or dropped (LTE disabled), or nothing was requested.
 * @retval -ETIMEDOUT Still requested or running.
 */
int ctr_lte_v2_wait_for_scan(k_timeout_t timeout);

/**
 * @brief Get the last ended scan (valid once one has ended); a running scan keeps it until its
 *        own end.
 */
int ctr_lte_v2_get_scan_result(struct ctr_lte_v2_scan_result *result);

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

/** EMM cause text (3GPP TS 24.301 9.9.3.9), e.g. 15 "No suitable cells in tracking area";
 *  "unknown" without CONFIG_CTR_LTE_V2_CAUSE_STR. */
const char *ctr_lte_v2_str_emm_cause(int cause);
/** Hint what to do about an EMM cause, NULL if there is none. */
const char *ctr_lte_v2_str_emm_cause_hint(int cause);
/** ESM cause text (3GPP TS 24.301 9.9.4.4), e.g. 27 "Missing or unknown APN";
 *  "unknown" without CONFIG_CTR_LTE_V2_CAUSE_STR. */
const char *ctr_lte_v2_str_esm_cause(int cause);
/** Hint what to do about an ESM cause, NULL if there is none. */
const char *ctr_lte_v2_str_esm_cause_hint(int cause);

/** @} */ /* end of group hio_lte_modem */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LTE_V2_H_ */
