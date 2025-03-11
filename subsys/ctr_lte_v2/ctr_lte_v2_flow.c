/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_parse.h"
#include "ctr_lte_v2_state.h"
#include "ctr_lte_v2_talk.h"
#include "ctr_lte_v2_tok.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>
#include <chester/ctr_util.h>
#include <chester/ctr_info.h>
#include <chester/drivers/ctr_lte_link.h>
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/net/socket_ncs.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SO_RAI_NO_DATA  50
#define SO_RAI_LAST     51
#define SO_RAI_ONE_RESP 52

LOG_MODULE_REGISTER(ctr_lte_v2_flow, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define XSLEEP_PAUSE         K_MSEC(100)
#define BOOT_TIMEOUT         K_SECONDS(5)
#define SEND_CSCON_1_TIMEOUT K_SECONDS(30)
#define SEND_CSCON_0_TIMEOUT K_SECONDS(30)

#define XRECVFROM_TIMEOUT_SEC 5

static struct ctr_lte_v2_talk m_talk;

static const struct device *dev_lte_if = DEVICE_DT_GET(DT_CHOSEN(ctr_lte_link));
static const struct device *dev_rfmux = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

static K_SEM_DEFINE(m_ready_sem, 0, 1);

#define EVENT_CSCON_0          BIT(0)
#define EVENT_CSCON_1          BIT(1)
#define EVENT_XMODEMSLEEP      BIT(2)
#define EVENT_SIMDETECTED      BIT(3)
#define EVENT_MDMEV_RESET_LOOP BIT(4)

static K_EVENT_DEFINE(m_flow_events);

static atomic_t m_started;
static ctr_lte_v2_flow_event_delegate_cb m_event_delegate_cb = NULL;
static ctr_lte_v2_flow_bypass_cb m_bypass_cb = NULL;
static void *m_bypass_user_data = NULL;

static void process_urc(const char *line)
{
	LOG_INF("URC: %s", line);

	if (g_ctr_lte_v2_config.test) {
		if (!strcmp(line, "Ready")) {
			k_sem_give(&m_ready_sem);
		}

		if (m_bypass_cb) {
			m_bypass_cb(m_bypass_user_data, (const uint8_t *)line, strlen(line));
		}

		return;
	}

	if (!strcmp(line, "Ready")) {
		m_event_delegate_cb(CTR_LTE_V2_EVENT_READY);

	} else if (!strcmp(line, "%XSIM: 1")) {
		m_event_delegate_cb(CTR_LTE_V2_EVENT_SIMDETECTED);

	} else if (!strncmp(line, "%XTIME:", 7)) {
		m_event_delegate_cb(CTR_LTE_V2_EVENT_XTIME);

	} else if (!strncmp(line, "+CEREG: ", 8)) {
		int ret;
		struct ctr_lte_v2_cereg_param cereg_param;

		ret = ctr_lte_v2_parse_urc_cereg(&line[8], &cereg_param);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_parse_urc_cereg` failed: %d", ret);
			return;
		}

		ctr_lte_v2_state_set_cereg_param(&cereg_param);

		if (cereg_param.stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME ||
		    cereg_param.stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING) {
			m_event_delegate_cb(CTR_LTE_V2_EVENT_REGISTERED);
		} else {
			m_event_delegate_cb(CTR_LTE_V2_EVENT_DEREGISTERED);
		}

	} else if (!strncmp(line, "%MDMEV: ", 8)) {
		if (!strncmp(&line[8], "RESET LOOP", 10)) {
			LOG_WRN("Modem reset loop detected");
			m_event_delegate_cb(CTR_LTE_V2_EVENT_RESET_LOOP);
		}
	} else if (!strncmp(line, "+CSCON: 0", 9)) {
		m_event_delegate_cb(CTR_LTE_V2_EVENT_CSCON_0);

	} else if (!strncmp(line, "+CSCON: 1", 9)) {
		m_event_delegate_cb(CTR_LTE_V2_EVENT_CSCON_1);

	} else if (!strncmp(line, "%XMODEMSLEEP: ", 14)) {
		int ret, p1, p2;
		ret = ctr_lte_v2_parse_urc_xmodemsleep(line + 14, &p1, &p2);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_parse_urc_xmodemsleep` failed: %d", ret);
			return;
		}
		if (p2 > 0) {
			m_event_delegate_cb(CTR_LTE_V2_EVENT_XMODMSLEEEP);
		}
	} else if (!strncmp(line, "#XGPS: ", 7)) {
		if (strcmp(&line[7], "1,0") != 0 && strcmp(&line[7], "1,1") != 0) {
			struct ctr_lte_v2_gnss_update update;
			int ret = ctr_lte_v2_parse_urc_xgps(&line[7], &update);
			if (ret) {
				LOG_WRN("Call `ctr_lte_v2_parse_urc_xgps` failed: %d", ret);
				return;
			}
			ctr_lte_v2_state_set_gnss_update(&update);
			m_event_delegate_cb(CTR_LTE_V2_EVENT_XGPS);
		}
	}
}

static void ctr_lte_v2_talk_event_handler(struct ctr_lte_v2_talk *talk, const char *line,
					  void *user_data)
{
	process_urc(line);
}

static void ctr_lte_link_event_handler(const struct device *dev, enum ctr_lte_link_event event,
				       void *user_data)
{
	int ret;

	switch (event) {
	case CTR_LTE_LINK_EVENT_RX_LINE:
		for (;;) {
			char *line;
			ret = ctr_lte_link_recv_line(dev, K_NO_WAIT, &line);
			if (ret) {
				LOG_ERR("Call `ctr_lte_link_recv_line` failed: %d", ret);
				break;
			}

			if (!line) {
				break;
			}

			process_urc(line);

			ret = ctr_lte_link_free_line(dev, (char *)line);
			if (ret) {
				LOG_ERR("Call `ctr_lte_link_free_line` failed: %d", ret);
			}
		}
		break;
	case CTR_LTE_LINK_EVENT_RX_DATA:
		if (m_bypass_cb) {
			uint8_t buf[64];
			size_t len = 0;
			size_t read;
			for (;;) {
				ret = ctr_lte_link_recv_data(dev, K_NO_WAIT, &buf[len], 1, &read);
				if (ret) {
					break;
				}
				if (read == 1) {
					len++;
				} else {
					break;
				}
				if (len == sizeof(buf)) {
					m_bypass_cb(m_bypass_user_data, buf, len);
					len = 0;
				}
			}
			if (len > 0) {
				m_bypass_cb(m_bypass_user_data, buf, len);
			}
		}
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static void str_remove_trailin_quotes(char *str)
{
	int l = strlen(str);
	if (l > 0 && str[0] == '"' && str[l - 1] == '"') {
		str[l - 1] = '\0';        /* Remove trailing quote */
		memmove(str, str + 1, l); /* Remove leading quote */
	}
}

static int fill_bands(char *bands)
{
	size_t len = strlen(bands);
	const char *p = g_ctr_lte_v2_config.bands;
	bool def;
	long band;
	while (p) {
		if (!(p = ctr_lte_v2_tok_num(p, &def, &band)) || !def || band < 0 || band > 255) {
			LOG_ERR("Invalid number format");
			return -EINVAL;
		}

		LOG_INF("Band: %ld", band);

		int n = len - band; /* band 1 is first 1 in bands from right */
		if (n < 0) {
			LOG_ERR("Invalid band number");
			return -EINVAL;
		}

		bands[n] = '1';

		if (ctr_lte_v2_tok_end(p)) {
			break;
		}

		if (!(p = ctr_lte_v2_tok_sep(p))) {
			LOG_ERR("Expected comma");
			return -EINVAL;
		}
	}

	return 0;
}

int ctr_lte_v2_flow_prepare(void)
{
	int ret;

	ret = ctr_lte_v2_talk_at(&m_talk);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at` failed: %d", ret);
		return ret;
	}

	if (g_ctr_lte_v2_config.modemtrace) {
		ret = ctr_lte_v2_talk_at_xmodemtrace(&m_talk);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_xmodemtrace` failed: %d", ret);
			return ret;
		}
	}

	char cgsn[64] = {0};
	ret = ctr_lte_v2_talk_at_cgsn(&m_talk, cgsn, sizeof(cgsn));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cgsn` failed: %d", ret);
		return ret;
	}

	str_remove_trailin_quotes(cgsn);

	LOG_INF("CGSN: %s", cgsn);

	ctr_lte_v2_state_set_imei(strtoull(cgsn, NULL, 10));

	char hw_version[64] = {0};
	ret = ctr_lte_v2_talk_at_hwversion(&m_talk, hw_version, sizeof(hw_version));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_hwversion` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW version: %s", hw_version);

	char sw_version[64] = {0};
	ret = ctr_lte_v2_talk_at_shortswver(&m_talk, sw_version, sizeof(sw_version));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_shortswver` failed: %d", ret);
		return ret;
	}

	LOG_INF("SW version: %s", sw_version);

	char slm_version[64] = {0};
	ret = ctr_lte_v2_talk_at_xslmver(&m_talk, slm_version, sizeof(slm_version));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xslmver` failed: %d", ret);
		return ret;
	}

	LOG_INF("SLM version: %s", slm_version);

	char xversion[64] = {0};
	ret = ctr_lte_v2_talk_at_xversion(&m_talk, xversion, sizeof(xversion));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xversion` failed: %d", ret);
		return ret;
	}

	str_remove_trailin_quotes(xversion);

	LOG_INF("Version: %s", xversion);

	ctr_lte_v2_state_set_modem_fw_version(xversion);

	ret = ctr_lte_v2_talk_at_cfun(&m_talk, 0);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xpofwarn(&m_talk, 1, 30);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xpofwarn` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xtemphighlvl(&m_talk, 70);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xtemphighlvl` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xtemp(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xtemp` failed: %d", ret);
		return ret;
	}

	int gnss_mode = 0;
#if defined(CONFIG_CTR_LTE_V2_GNSS)
	gnss_mode = 1;
#endif

	char *pos_let_m = strstr(g_ctr_lte_v2_config.mode, "lte-m");
	char *pos_nb_iot = strstr(g_ctr_lte_v2_config.mode, "nb-iot");

	int lte_m_mode = pos_let_m ? 1 : 0;
	int nb_iot_mode = pos_nb_iot ? 1 : 0;
	int preference = 0;
	if (pos_let_m && pos_nb_iot) {
		preference = pos_let_m < pos_nb_iot ? 1 : 2;
	}

	ret = ctr_lte_v2_talk_at_xsystemmode(&m_talk, lte_m_mode, nb_iot_mode, gnss_mode,
					     preference);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xsystemmode` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xdataprfl(&m_talk, 0);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xdataprfl` failed: %d", ret);
		return ret;
	}

	if (!strlen(g_ctr_lte_v2_config.bands)) {
		ret = ctr_lte_v2_talk_at_xbandlock(&m_talk, 0, NULL);
	} else {
		char bands[] =
			"00000000000000000000000000000000000000000000000000000000000000000000"
			"00000000000000000000";

		ret = fill_bands(bands);
		if (ret) {
			LOG_ERR("Call `fill_bands` failed: %d", ret);
			return ret;
		}
		ret = ctr_lte_v2_talk_at_xbandlock(&m_talk, 1, bands);
	}
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xbandlock` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xsim(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xsim` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_xnettime(&m_talk, 1, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xnettime` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_mdmev(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_mdmev` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_rel14feat(&m_talk, 1, 1, 1, 1, 0);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_rel14feat` failed: %d", ret);
		return ret;
	}

	/* TODO Make optional */
	ret = ctr_lte_v2_talk_at_rai(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_rai` failed: %d", ret);
		return ret;
	}

	/* TODO Configurable? */
	ret = ctr_lte_v2_talk_at_cpsms(&m_talk, (int[]){1}, "00111000", "00000000");
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cpsms` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_ceppi(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_ceppi` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_cereg(&m_talk, 5);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cereg` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_cgerep(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cgerep` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_cmee(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmee` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_at_cnec(&m_talk, 24);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cnec` failed: %d", ret);
		return ret;
	}

	if (gnss_mode) {
		ctr_lte_v2_talk_at_cmd(&m_talk, "AT%XCOEX0=1,1,1565,1586");
	}

	ret = ctr_lte_v2_talk_at_cscon(&m_talk, 1);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cscon` failed: %d", ret);
		return ret;
	}

	if (!strlen(g_ctr_lte_v2_config.network)) {
		ret = ctr_lte_v2_talk_at_cops(&m_talk, 0, NULL, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cops` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_v2_talk_at_cops(&m_talk, 1, (int[]){2}, g_ctr_lte_v2_config.network);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cops` failed: %d", ret);
			return ret;
		}
	}

	if (!strlen(g_ctr_lte_v2_config.apn)) {
		ret = ctr_lte_v2_talk_at_cgdcont(&m_talk, 0, "IP", NULL);
	} else {
		ret = ctr_lte_v2_talk_at_cgdcont(&m_talk, 0, "IP", g_ctr_lte_v2_config.apn);
	}
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cgdcont` failed: %d", ret);
		return ret;
	}

	if (g_ctr_lte_v2_config.auth == CTR_LTE_V2_CONFIG_AUTH_PAP ||
	    g_ctr_lte_v2_config.auth == CTR_LTE_V2_CONFIG_AUTH_CHAP) {
		int protocol = g_ctr_lte_v2_config.auth == CTR_LTE_V2_CONFIG_AUTH_PAP ? 1 : 2;
		ret = ctr_lte_v2_talk_at_cgauth(&m_talk, 0, &protocol, g_ctr_lte_v2_config.username,
						g_ctr_lte_v2_config.password);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cgauth` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_v2_talk_at_cgauth(&m_talk, 0, (int[]){0}, NULL, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cgauth` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_lte_v2_talk_at_xmodemsleep(&m_talk, 1, (int[]){500}, (int[]){10240});
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xmodemsleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_cfun(int cfun)
{
	int ret;

	ret = ctr_lte_v2_talk_at_cfun(&m_talk, cfun);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_sim_info(void)
{
	int ret;
	char cimi[64] = {0};
	uint64_t imsi, imsi_test = 0;

	for (int i = 0; i < 10; i++) {
		memset(cimi, 0, sizeof(cimi));
		ret = ctr_lte_v2_talk_at_cimi(&m_talk, cimi, sizeof(cimi));
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cimi` failed: %d", ret);
			return ret;
		}

		imsi = strtoull(cimi, NULL, 10);

		if (imsi != 0 && imsi == imsi_test) {
			break;
		}

		imsi_test = imsi;
	}

	LOG_INF("CIMI: %llu", imsi);

	ctr_lte_v2_state_set_imsi(imsi);

	char iccid[64] = {0};
	ret = ctr_lte_v2_talk_at_iccid(&m_talk, iccid, sizeof(iccid));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_iccid` failed: %d", ret);
		return ret;
	}

	if (strlen(iccid) < 18 || strlen(iccid) > 22) {
		LOG_ERR("Invalid ICCID: %s", iccid);
		return -EINVAL;
	}

	LOG_INF("ICCID: %s", iccid);

	ctr_lte_v2_state_set_iccid(iccid);

	return 0;
}

int ctr_lte_v2_flow_sim_fplmn(void)
{
	/* Test FPLMN (forbidden network) list on a SIM */
	int ret;
	char crsm_144[32];

	ret = ctr_lte_v2_talk_crsm_176(&m_talk, crsm_144, sizeof(crsm_144));
	if (ret) {
		LOG_ERR("Call `ctr_lte_talk_at_crsm_176` failed: %d", ret);
		return ret;
	}
	if (strcmp(crsm_144, "\"FFFFFFFFFFFFFFFFFFFFFFFF\"")) {
		LOG_WRN("Found forbidden network(s) - erasing");

		/* FPLMN erase */
		ret = ctr_lte_v2_talk_crsm_214(&m_talk);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_crsm_214` failed: %d", ret);
			return ret;
		}

		ret = ctr_lte_v2_talk_at_cfun(&m_talk, 4);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cfun: 4` failed: %d", ret);
			return ret;
		}

		k_sleep(K_MSEC(100));

		ret = ctr_lte_v2_talk_at_cfun(&m_talk, 1);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_cfun: 1` failed: %d", ret);
			return ret;
		}

		return -EAGAIN;
	}

	return 0;
}

int ctr_lte_v2_flow_open_socket(void)
{
	int ret;

	char cops[64];
	ret = ctr_lte_v2_talk_at_cops_q(&m_talk, cops, sizeof(cops));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cops_q` failed: %d", ret);
		return ret;
	}

	LOG_INF("COPS: %s", cops);

	ctr_lte_v2_talk_at_cmd(&m_talk, "AT%XCBAND");
	ctr_lte_v2_talk_at_cmd(&m_talk, "AT+CEINFO?");
	ctr_lte_v2_talk_at_cmd(&m_talk, "AT+CGDCONT?"); /* TODO: check ip */

	char xsocket[64];
	ret = ctr_lte_v2_talk_at_xsocket(&m_talk, 1, (int[]){2}, (int[]){0}, xsocket,
					 sizeof(xsocket));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xsocket` failed: %d", ret);
		return ret;
	}

	LOG_INF("XSOCKET: %s", xsocket);

	struct xsocket_set_param xsocket_param;
	ret = ctr_lte_v2_parse_xsocket_set(xsocket, &xsocket_param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_parse_xsocket_set` failed: %d", ret);
		return ret;
	}

	if (xsocket_param.type != XSOCKET_TYPE_DGRAM ||
	    xsocket_param.protocol != XSOCKET_PROTOCOL_UDP) {
		LOG_ERR("Unexpected socket response: %s", xsocket);
		return -ENOTCONN;
	}

	char at_cmd[15 + sizeof(g_ctr_lte_v2_config.addr) + 5 + 1] = {0};
	char ar_response[64] = {0};
	snprintf(at_cmd, sizeof(at_cmd), "AT#XCONNECT=\"%s\",%u", g_ctr_lte_v2_config.addr,
		 CONFIG_CTR_LTE_V2_PORT);

	ret = ctr_lte_v2_talk_at_cmd_with_resp(&m_talk, at_cmd, ar_response, sizeof(ar_response));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp` failed: %d", ret);
		return ret;
	}

	if (strcmp(ar_response, "#XCONNECT: 1") != 0) {
		LOG_ERR("Unexpected XCONNECT response: %s", ar_response);
		return -ENOTCONN;
	}

	return 0;
}

int ctr_lte_v2_flow_check(void)
{
	int ret;

	char resp[128] = {0};

	/* Check functional mode */
	ret = ctr_lte_v2_talk_at_cmd_with_resp_prefix(&m_talk, "AT+CFUN?", resp, sizeof(resp),
						      "+CFUN: ");

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp_prefix` AT+CFUN? failed: %d", ret);
		return ret;
	}

	if (strcmp(resp, "1") != 0) {
		LOG_ERR("Unexpected CFUN response: %s", resp);
		return -ENOTCONN;
	}

	/* Check network registration status */
	ret = ctr_lte_v2_talk_at_cmd_with_resp_prefix(&m_talk, "AT+CEREG?", resp, sizeof(resp),
						      "+CEREG: ");

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp_prefix` AT+CEREG? failed: %d", ret);
		return ret;
	}

	if (resp[0] == '0') {
		LOG_ERR("CEREG unsubscribe unsolicited result codes");
		return -ENOTCONN;
	}

	struct ctr_lte_v2_cereg_param cereg_param;

	ret = ctr_lte_v2_parse_urc_cereg(resp + 2, &cereg_param);
	if (ret) {
		LOG_WRN("Call `ctr_lte_v2_parse_urc_cereg` failed: %d", ret);
		return ret;
	}

	ctr_lte_v2_state_set_cereg_param(&cereg_param);

	if (cereg_param.stat != CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME &&
	    cereg_param.stat != CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING) {
		LOG_ERR("Unexpected CEREG response: %s", resp);
		return -ENOTCONN;
	}

	/* Check if PDN is active */
	ret = ctr_lte_v2_talk_at_cmd_with_resp_prefix(&m_talk, "AT+CGATT?", resp, sizeof(resp),
						      "+CGATT: ");

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp_prefix` AT+CGATT? failed: %d", ret);
		return ret;
	}

	if (strcmp(resp, "1") != 0) {
		LOG_ERR("Unexpected CGATT response: %s", resp);
		return -ENOTCONN;
	}

	/* Check PDN connections */
	ret = ctr_lte_v2_talk_at_cmd_with_resp_prefix(&m_talk, "AT+CGACT?", resp, sizeof(resp),
						      "+CGACT: ");

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp_prefix` AT+CGACT? failed: %d", ret);
		return ret;
	}

	if (strcmp(resp, "0,1") != 0) {
		LOG_ERR("Unexpected CGACT response: %s", resp);
		return -ENOTCONN;
	}

	/* Check socket */
	ret = ctr_lte_v2_talk_at_cmd_with_resp_prefix(&m_talk, "AT#XSOCKET?", resp, sizeof(resp),
						      "#XSOCKET: ");

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_cmd_with_resp_prefix` AT#XSOCKET failed: %d",
			ret);
		return ret;
	}

	struct xsocket_get_param xsocket_param;
	ret = ctr_lte_v2_parse_xsocket_get(resp, &xsocket_param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_parse_xsocket_get` failed: %d", ret);
		return ret;
	}

	if (xsocket_param.type != XSOCKET_TYPE_DGRAM && xsocket_param.role != XSOCKET_ROLE_CLIENT) {
		LOG_ERR("Unexpected XSOCKET response: %s", resp);
		return -ENOTCONN;
	}

	return 0;
}

int ctr_lte_v2_flow_send(const struct ctr_lte_v2_send_recv_param *param)
{
	int ret;

	if (param->rai) {
		ret = ctr_lte_v2_talk_at_xsocketopt(
			&m_talk, 1, param->recv_buf ? SO_RAI_ONE_RESP : SO_RAI_LAST, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_xsocketopt` failed: %d", ret);
			return ret;
		}
	}

	if (param->send_as_string) {
		ret = ctr_lte_v2_talk_at_xsend_string(&m_talk, param->send_buf, param->send_len);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_xsend_string` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_v2_talk_at_xsend(&m_talk, param->send_buf, param->send_len);
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_talk_at_xsend` failed: %d", ret);
			return ret;
		}
	}

	if (param->rai && !param->recv_buf) { /* RAI and no receive buffer */
		ret = ctr_lte_v2_talk_at_xsocketopt(&m_talk, 1, SO_RAI_NO_DATA, NULL);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_talk_at_xsocketopt 1,50` failed: %d", ret);
		}
	}

	return 0;
}

int ctr_lte_v2_flow_recv(const struct ctr_lte_v2_send_recv_param *param)
{
	if (!param->recv_buf || !param->recv_size || !param->recv_len) {
		return -EINVAL;
	}

	int ret;

	ret = ctr_lte_v2_talk_at_xrecv(&m_talk, XRECVFROM_TIMEOUT_SEC, param->recv_buf,
				       param->recv_size, param->recv_len);

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_xrecv` failed: %d", ret);
		return ret;
	}

	if (param->rai) {
		ret = ctr_lte_v2_talk_at_xsocketopt(&m_talk, 1, SO_RAI_NO_DATA, NULL);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_talk_at_xsocketopt 1,50` failed: %d", ret);
		}
	}

	return 0;
}

int ctr_lte_v2_flow_reset(void)
{
	int ret;

	ret = ctr_lte_link_reset(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_enable(bool wakeup)
{
	int ret;

	if (atomic_get(&m_started)) {
		return 0;
	}

	if (!device_is_ready(dev_lte_if)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_lte_link_enable_uart(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_enable_uart` failed: %d", ret);
		return ret;
	}

	if (wakeup) {
		ret = ctr_lte_link_wake_up(dev_lte_if);
		if (ret) {
			LOG_ERR("Call `ctr_lte_link_wake_up` failed: %d", ret);
			return ret;
		}
	}

	atomic_set(&m_started, true);

	/* TODO Reset MODEMSLEEP */

	return 0;
}

int ctr_lte_v2_flow_disable(bool send_sleep)
{
	int ret;

	if (!atomic_get(&m_started)) {
		return 0;
	}

	atomic_set(&m_started, false);

	if (send_sleep) {
		ret = ctr_lte_v2_talk_at_xsleep(&m_talk, 2);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_talk_at_xsleep` failed: %d", ret);
		}

		k_sleep(XSLEEP_PAUSE);
	}

	ret = ctr_lte_link_disable_uart(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_disable_uart` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_rfmux_acquire(void)
{
	int ret;
	ret = ctr_rfmux_acquire(dev_rfmux);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_acquire` failed: %d", ret);
		return ret;
	}

	enum ctr_rfmux_antenna antenna;

	if (g_ctr_lte_v2_config.antenna == CTR_LTE_V2_CONFIG_ANTENNA_EXT) {
		antenna = CTR_RFMUX_ANTENNA_EXT;
	} else {
		antenna = CTR_RFMUX_ANTENNA_INT;
	}

	ret = ctr_rfmux_set_antenna(dev_rfmux, antenna);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_antenna` failed: %d", ret);
		return ret;
	}

	ret = ctr_rfmux_set_interface(dev_rfmux, CTR_RFMUX_INTERFACE_LTE);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_interface` failed: %d", ret);
		return ret;
	}
	return 0;
}

int ctr_lte_v2_flow_rfmux_release(void)
{
	int ret;

	ret = ctr_rfmux_set_interface(dev_rfmux, CTR_RFMUX_INTERFACE_NONE);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_interface` failed: %d", ret);
		return ret;
	}

	ret = ctr_rfmux_set_antenna(dev_rfmux, CTR_RFMUX_ANTENNA_NONE);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_set_antenna` failed: %d", ret);
		return ret;
	}

	ret = ctr_rfmux_release(dev_rfmux);
	if (ret) {
		LOG_ERR("Call `ctr_rfmux_release` failed: %d", ret);
		return ret;
	}
	return 0;
}

int ctr_lte_v2_flow_coneval(void)
{
	int ret;

	char buf[128] = {0};

	ret = ctr_lte_v2_talk_at_coneval(&m_talk, buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_at_coneval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_v2_conn_param conn_param;

	ret = ctr_lte_v2_parse_coneval(buf, &conn_param);

	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_parse_coneval` failed: %d", ret);
		return ret;
	}

	if (conn_param.result != 0) {
		LOG_WRN("Connection evaluation: %s",
			ctr_lte_v2_coneval_result_str(conn_param.result));
	}

	ctr_lte_v2_state_set_conn_param(&conn_param);

	return 0;
}

int ctr_lte_v2_flow_init(ctr_lte_v2_flow_event_delegate_cb cb)
{
	int ret;

	ret = ctr_lte_v2_talk_init(&m_talk, dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_v2_talk_set_callback(&m_talk, ctr_lte_v2_talk_event_handler,
					   (void *)dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_link_set_callback(dev_lte_if, ctr_lte_link_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_set_callback` failed: %d", ret);
		return ret;
	}

	m_event_delegate_cb = cb;

	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("LTE Test mode enabled, skipping lte link reset");
	} else {
		ret = ctr_lte_link_reset(dev_lte_if);
		if (ret) {
			LOG_ERR("Call `ctr_lte_link_reset` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

int ctr_lte_v2_flow_wake_up_and_wait_on_ready(void)
{
	int ret;

	if (!g_ctr_lte_v2_config.test) {
		LOG_WRN("Not in test mode");
		return -ENOTSUP;
	}

	if (!atomic_get(&m_started)) {
		return -ENOTCONN;
	}

	k_sem_reset(&m_ready_sem);

	ret = ctr_lte_link_wake_up(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_wake_up` failed: %d", ret);
		return ret;
	}

	ret = k_sem_take(&m_ready_sem, BOOT_TIMEOUT);
	if (ret) {
		if (ret != -EAGAIN) {
			LOG_WRN("Call `k_sem_take` failed: %d", ret);
		}
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_cmd_without_response(const char *s)
{
	int ret;

	if (!s) {
		return -EINVAL;
	}

	if (!atomic_get(&m_started)) {
		return -ENOTCONN;
	}

	ret = ctr_lte_v2_talk_(&m_talk, s);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_talk_` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_flow_bypass_set_cb(ctr_lte_v2_flow_bypass_cb cb, void *user_data)
{
	m_bypass_cb = cb;
	m_bypass_user_data = user_data;

	int ret;

	if (cb) {
		ret = ctr_lte_link_enter_data_mode(dev_lte_if);
		if (ret) {
			LOG_ERR("Call `ctr_lte_link_enter_data_mode` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_link_exit_data_mode(dev_lte_if);
		if (ret) {
			LOG_ERR("Call `ctr_lte_link_exit_data_mode` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

int ctr_lte_v2_flow_bypass_write(const uint8_t *data, const size_t len)
{
	if (!data || !len) {
		return -EINVAL;
	}

	if (!atomic_get(&m_started)) {
		return -ENOTCONN;
	}

	int ret = ctr_lte_link_send_data(dev_lte_if, K_SECONDS(1), data, len);
	if (ret) {
		LOG_ERR("Call `ctr_lte_link_send_data` failed: %d", ret);
		return ret;
	}

	return 0;
}
