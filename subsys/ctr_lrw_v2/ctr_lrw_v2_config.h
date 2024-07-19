#ifndef CHESTER_SUBSYS_CTR_LRW_V2_CONFIG_H_
#define CHESTER_SUBSYS_CTR_LRW_V2_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lrw_v2_config_antenna {
	CTR_LRW_V2_CONFIG_ANTENNA_INT = 0,
	CTR_LRW_V2_CONFIG_ANTENNA_EXT = 1,
};

enum ctr_lrw_v2_config_band {
	CTR_LRW_V2_CONFIG_BAND_AS923 = 0,
	CTR_LRW_V2_CONFIG_BAND_AU915 = 1,
	CTR_LRW_V2_CONFIG_BAND_EU868 = 5,
	CTR_LRW_V2_CONFIG_BAND_KR920 = 6,
	CTR_LRW_V2_CONFIG_BAND_IN865 = 7,
	CTR_LRW_V2_CONFIG_BAND_US915 = 8,
};

enum ctr_lrw_v2_config_class {
	CTR_LRW_V2_CONFIG_CLASS_A = 0,
	CTR_LRW_V2_CONFIG_CLASS_C = 2,
};

enum ctr_lrw_v2_config_mode {
	CTR_LRW_V2_CONFIG_MODE_ABP = 0,
	CTR_LRW_V2_CONFIG_MODE_OTAA = 1,
};

enum ctr_lrw_v2_config_network {
	CTR_LRW_V2_CONFIG_NETWORK_PRIVATE = 0,
	CTR_LRW_V2_CONFIG_NETWORK_PUBLIC = 1,
};

struct ctr_lrw_v2_config {
	bool test;
	enum ctr_lrw_v2_config_antenna antenna;
	enum ctr_lrw_v2_config_band band;
	char chmask[24 + 1];
	enum ctr_lrw_v2_config_class class;
	enum ctr_lrw_v2_config_mode mode;
	enum ctr_lrw_v2_config_network nwk;
	bool adr;
	int datarate;
	bool dutycycle;
	uint8_t devaddr[4];
	uint8_t deveui[8];
	uint8_t joineui[8];
	uint8_t appkey[16];
	uint8_t nwkskey[16];
	uint8_t appskey[16];
};

extern struct ctr_lrw_v2_config g_ctr_lrw_v2_config;

int ctr_lrw_v2_config_cmd_show(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_test(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_antenna(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_band(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_chmask(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_class(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_mode(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_nwk(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_adr(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_datarate(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_dutycycle(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_devaddr(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_deveui(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_joineui(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_appkey(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_nwkskey(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_config_cmd_appskey(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LRW_V2_CONFIG_H_ */
