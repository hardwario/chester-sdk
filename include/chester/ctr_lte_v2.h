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

struct ctr_lte_v2_conn_param {
	bool valid;
	int result;
	int eest;
	int ecl;
	int rsrp;
	int rsrq;
	int snr;
	int plmn;
	int cid;
	int band;
	int earfcn;
};

enum ctr_lte_v2_cereg_param_stat {
	CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED = 0,
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME = 1,
	CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING = 2,
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED = 3,
	CTR_LTE_V2_CEREG_PARAM_STAT_UNKNOWN = 4,
	CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING = 5,
	CTR_LTE_V2_CEREG_PARAM_STAT_SIM_FAILURE = 90,
};

enum ctr_lte_v2_cereg_param_act {
	CTR_LTE_V2_CEREG_PARAM_ACT_UNKNOWN = 0,
	CTR_LTE_V2_CEREG_PARAM_ACT_LTE = 7,
	CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT = 9,
};

struct ctr_lte_v2_cereg_param {
	bool valid;
	enum ctr_lte_v2_cereg_param_stat stat;
	char tac[5];                         // Tracking Area Code (TAC). (hexadecimal format.)
	int cid;                             // Cell Identity (CI) (E-UTRAN cell ID.)
	enum ctr_lte_v2_cereg_param_act act; // Access Technology (AcT).
};

struct ctr_lte_v2_send_recv_param {
	bool rai;
	const void *send_buf;
	size_t send_len;
	void *recv_buf;
	size_t recv_size;
	size_t *recv_len;
	k_timeout_t timeout;
};

struct ctr_lte_v2_metrics {
	uint32_t uplink_count;
	uint32_t uplink_bytes;
	uint32_t uplink_errors;
	int64_t uplink_last_ts;
	uint32_t downlink_count;
	uint32_t downlink_bytes;
	uint32_t downlink_errors;
	int64_t downlink_last_ts;
};

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

int ctr_lte_v2_enable(void);
int ctr_lte_v2_wait_for_connected(k_timeout_t timeout);
int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param);

// get information
int ctr_lte_v2_get_imei(uint64_t *imei);
int ctr_lte_v2_get_imsi(uint64_t *imsi);
int ctr_lte_v2_get_iccid(char **iccid);
int ctr_lte_v2_get_modem_fw_version(char **version);
int ctr_lte_v2_get_conn_param(struct ctr_lte_v2_conn_param *param);
int ctr_lte_v2_get_cereg_param(struct ctr_lte_v2_cereg_param *param);
int ctr_lte_v2_get_metrics(struct ctr_lte_v2_metrics *metrics);

const char *ctr_lte_v2_coneval_result_str(int result);

int ctr_lte_v2_is_attached(bool *attached);

#if defined(CONFIG_CTR_LTE_V2_GNSS)
int ctr_lte_v2_gnss_set_enable(bool enable);
int ctr_lte_v2_gnss_get_enable(bool *enable);
int ctr_lte_v2_gnss_set_handler(ctr_lte_v2_gnss_cb callback, void *user_data);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LTE_V2_H_ */
