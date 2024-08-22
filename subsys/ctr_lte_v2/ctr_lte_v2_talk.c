/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_parse.h"
#include "ctr_lte_v2_talk.h"

/* CHESTER includes */
#include <chester/drivers/ctr_lte_link.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2_talk, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define SEND_TIMEOUT             K_SECONDS(1)
#define RESPONSE_TIMEOUT_S       K_SECONDS(3)
#define RESPONSE_TIMEOUT_L       K_SECONDS(30)
#define RESPONSE_TIMEOUT_CONEVAL K_SECONDS(30)

#define DIALOG_PROLOG bool in_dialog = false;

#define DIALOG_EPILOG return 0;

#define DIALOG_ENTER()                                                                             \
	do {                                                                                       \
		int ret = ctr_lte_link_enter_dialog(talk->dev);                                    \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_api_enter_dialog` failed: %d", ret);           \
			return ret;                                                                \
		}                                                                                  \
		in_dialog = true;                                                                  \
	} while (0)

#define DIALOG_EXIT()                                                                              \
	do {                                                                                       \
		int ret = ctr_lte_link_exit_dialog(talk->dev);                                     \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_exit_dialog` failed: %d", ret);                \
			return ret;                                                                \
		}                                                                                  \
		in_dialog = false;                                                                 \
	} while (0)

#define DIALOG_ABORT(code)                                                                         \
	do {                                                                                       \
		if (in_dialog) {                                                                   \
			int ret = ctr_lte_link_exit_dialog(talk->dev);                             \
			if (ret) {                                                                 \
				LOG_ERR("Call `ctr_lte_link_exit_dialog` failed: %d", ret);        \
			}                                                                          \
		}                                                                                  \
		return code;                                                                       \
	} while (0)

#define DIALOG_ENTER_DATA_MODE(abort)                                                              \
	do {                                                                                       \
		int ret = ctr_lte_link_enter_data_mode(talk->dev);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_enter_data_mode` failed: %d", ret);            \
			if (abort) {                                                               \
				DIALOG_ABORT(ret);                                                 \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define DIALOG_EXIT_DATA_MODE(abort)                                                               \
	do {                                                                                       \
		int ret = ctr_lte_link_exit_data_mode(talk->dev);                                  \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_exit_data_mode` failed: %d", ret);             \
			if (abort) {                                                               \
				DIALOG_ABORT(ret);                                                 \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define DIALOG_SEND_LINE(...)                                                                      \
	do {                                                                                       \
		int ret = ctr_lte_link_send_line(talk->dev, SEND_TIMEOUT, __VA_ARGS__);            \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_send_line` failed: %d", ret);                  \
			DIALOG_ABORT(ret);                                                         \
		}                                                                                  \
	} while (0)

#define DIALOG_RECV_LINE(timeout, line)                                                            \
	do {                                                                                       \
		int ret = ctr_lte_link_recv_line(talk->dev, timeout, line);                        \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_recv_line` failed: %d", ret);                  \
			DIALOG_ABORT(ret);                                                         \
		}                                                                                  \
	} while (0)

#define DIALOG_FREE_LINE(line, abort)                                                              \
	do {                                                                                       \
		int ret = ctr_lte_link_free_line(talk->dev, line);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_free_line` failed: %d", ret);                  \
			if (abort) {                                                               \
				DIALOG_ABORT(ret);                                                 \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define DIALOG_SEND_DATA(buf, len)                                                                 \
	do {                                                                                       \
		DIALOG_ENTER_DATA_MODE(true);                                                      \
		int ret = ctr_lte_link_send_data(talk->dev, SEND_TIMEOUT, buf, len);               \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_send_data` failed: %d", ret);                  \
			DIALOG_EXIT_DATA_MODE(false);                                              \
			DIALOG_ABORT(ret);                                                         \
		}                                                                                  \
		DIALOG_EXIT_DATA_MODE(true);                                                       \
	} while (0)

#define DIALOG_RECV_DATA(timeout, buf, size, len)                                                  \
	do {                                                                                       \
		DIALOG_ENTER_DATA_MODE(true);                                                      \
		int ret = ctr_lte_link_recv_data(talk->dev, timeout, buf, size, len);              \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lte_link_recv_data` failed: %d", ret);                  \
			DIALOG_EXIT_DATA_MODE(false);                                              \
			DIALOG_ABORT(ret);                                                         \
		}                                                                                  \
		DIALOG_EXIT_DATA_MODE(true);                                                       \
	} while (0)

#define DIALOG_LOOP_RUN(timeout, ...)                                                              \
	do {                                                                                       \
		__unused int gather_idx = 0;                                                       \
		k_timepoint_t end = sys_timepoint_calc(timeout);                                   \
		bool loop_run = true;                                                              \
		while (loop_run) {                                                                 \
			if (sys_timepoint_expired(end)) {                                          \
				DIALOG_ABORT(-ETIMEDOUT);                                          \
			}                                                                          \
			char *line;                                                                \
			DIALOG_RECV_LINE(sys_timepoint_timeout(end), &line);                       \
			if (!line) {                                                               \
				continue;                                                          \
			}                                                                          \
			bool process_urc = true;                                                   \
			__VA_ARGS__                                                                \
			if (process_urc && talk->user_cb) {                                        \
				talk->user_cb(talk, line, talk->user_data);                        \
			}                                                                          \
			DIALOG_FREE_LINE(line, true);                                              \
		}                                                                                  \
	} while (0)

#define DIALOG_LOOP_BREAK_ON_STR(str)                                                              \
	do {                                                                                       \
		if (!strcmp(line, str)) {                                                          \
			loop_run = false;                                                          \
			process_urc = false;                                                       \
		}                                                                                  \
	} while (0)

#define DIALOG_LOOP_ABORT_ON_PFX(pfx)                                                              \
	do {                                                                                       \
		if (!strncmp(line, pfx, strlen(pfx))) {                                            \
			DIALOG_FREE_LINE(line, false);                                             \
			DIALOG_ABORT(-EILSEQ);                                                     \
		}                                                                                  \
	} while (0)

#define DIALOG_LOOP_GATHER_GET_COUNT() gather_idx

#define DIALOG_LOOP_GATHER(buf, size)                                                              \
	do {                                                                                       \
		if (buf[0] == '+' || buf[0] == '%' || buf[0] == '#') {                             \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(line) >= size) {                                                \
				DIALOG_FREE_LINE(line, false);                                     \
				DIALOG_ABORT(-ENOSPC);                                             \
			} else {                                                                   \
				strcpy(buf, line);                                                 \
			}                                                                          \
		}                                                                                  \
		gather_idx++;                                                                      \
		process_urc = false;                                                               \
	} while (0)

#define DIALOG_LOOP_GATHER_PFX(pfx, buf, size)                                                     \
	do {                                                                                       \
		if (strncmp(line, pfx, strlen(pfx))) {                                             \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(&line[strlen(pfx)]) >= size) {                                  \
				DIALOG_FREE_LINE(line, false);                                     \
				DIALOG_ABORT(-ENOSPC);                                             \
			} else {                                                                   \
				strcpy(buf, &line[strlen(pfx)]);                                   \
				gather_idx++;                                                      \
				process_urc = false;                                               \
			}                                                                          \
		}                                                                                  \
		gather_idx++;                                                                      \
		process_urc = false;                                                               \
	} while (0)

#define DIALOG_LOOP_BREAK_ON_PFX(pfx, buf, size)                                                   \
	do {                                                                                       \
		if (strncmp(line, pfx, strlen(pfx))) {                                             \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(&line[strlen(pfx)]) >= size) {                                  \
				DIALOG_FREE_LINE(line, false);                                     \
				DIALOG_ABORT(-ENOSPC);                                             \
			} else {                                                                   \
				strcpy(buf, &line[strlen(pfx)]);                                   \
			}                                                                          \
		}                                                                                  \
		loop_run = false;                                                                  \
		process_urc = false;                                                               \
	} while (0)

int ctr_lte_v2_talk_init(struct ctr_lte_v2_talk *talk, const struct device *dev)
{
	talk->dev = dev;

	return 0;
}

int ctr_lte_v2_talk_set_callback(struct ctr_lte_v2_talk *talk, ctr_lte_v2_talk_cb user_cb,
				 void *user_data)
{
	talk->user_cb = user_cb;
	talk->user_data = user_data;

	return 0;
}

int ctr_lte_v2_talk_(struct ctr_lte_v2_talk *talk, const char *s)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("%s", s);
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cclk_q(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CCLK?");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("+CCLK: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_ceppi(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CEPPI=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cereg(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CEREG=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cfun(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CFUN=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cgauth(struct ctr_lte_v2_talk *talk, int p1, int *p2, const char *p3,
			      const char *p4)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2 && !p3 && !p4) {
		DIALOG_SEND_LINE("AT+CGAUTH=%d", p1);
	} else if (p2 && !p3 && !p4) {
		DIALOG_SEND_LINE("AT+CGAUTH=%d,%d", p1, *p2);
	} else if (p2 && p3 && !p4) {
		DIALOG_SEND_LINE("AT+CGAUTH=%d,%d,\"%s\"", p1, *p2, p3);
	} else if (p2 && p3 && p4) {
		DIALOG_SEND_LINE("AT+CGAUTH=%d,%d,\"%s\",\"%s\"", p1, *p2, p3,
				  p4);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cgdcont(struct ctr_lte_v2_talk *talk, int p1, const char *p2, const char *p3)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2 && !p3) {
		DIALOG_SEND_LINE("AT+CGDCONT=%d", p1);
	} else if (p2 && !p3) {
		DIALOG_SEND_LINE("AT+CGDCONT=%d,\"%s\"", p1, p2);
	} else if (p2 && p3) {
		DIALOG_SEND_LINE("AT+CGDCONT=%d,\"%s\",\"%s\"", p1, p2, p3);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cgerep(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CGEREP=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cgsn(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CGSN=1");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("+CGSN: ",buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cimi(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CIMI");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER(buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_iccid(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	return ctr_lte_v2_talk_at_cmd_with_resp_prefix(talk, "AT%XICCID", buf, size, "%XICCID: ");
}

int ctr_lte_v2_talk_at_cmee(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CMEE=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cnec(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CNEC=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_coneval(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%CONEVAL");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_CONEVAL, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("%CONEVAL: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cops_q(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+COPS?");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("+COPS: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cops(struct ctr_lte_v2_talk *talk, int p1, int *p2, const char *p3)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2 && !p3) {
		DIALOG_SEND_LINE("AT+COPS=%d", p1);
	} else if (p2 && !p3) {
		DIALOG_SEND_LINE("AT+COPS=%d,%d", p1, *p2);
	} else if (p2 && p3) {
		DIALOG_SEND_LINE("AT+COPS=%d,%d,\"%s\"", p1, *p2, p3);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cpsms(struct ctr_lte_v2_talk *talk, int *p1, const char *p2, const char *p3)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p1 && !p2 && !p3) {
		DIALOG_SEND_LINE("AT+CPSMS=");
	} else if (p1 && !p2 && !p3) {
		DIALOG_SEND_LINE("AT+CPSMS=%d", *p1);
	} else if (p1 && p2 && p3) {
		DIALOG_SEND_LINE("AT+CPSMS=%d,,,\"%s\",\"%s\"", *p1, p2, p3);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cscon(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CSCON=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_hwversion(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%HWVERSION");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("%HWVERSION: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_mdmev(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%MDMEV=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_rai(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%RAI=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_rel14feat(struct ctr_lte_v2_talk *talk, int p1, int p2, int p3, int p4,
				 int p5)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%REL14FEAT=%d,%d,%d,%d,%d", p1, p2, p3, p4, p5);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_shortswver(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%SHORTSWVER");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("%SHORTSWVER: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xbandlock(struct ctr_lte_v2_talk *talk, int p1, const char *p2)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2) {
		DIALOG_SEND_LINE("AT%%XBANDLOCK=%d", p1);
	} else {
		DIALOG_SEND_LINE("AT%%XBANDLOCK=%d,\"%s\"", p1, p2);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xdataprfl(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XDATAPRFL=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xmodemsleep(struct ctr_lte_v2_talk *talk, int p1, int *p2, int *p3)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2 && !p3) {
		DIALOG_SEND_LINE("AT%%XMODEMSLEEP=%d", p1);
	} else if (p2 && p3) {
		DIALOG_SEND_LINE("AT%%XMODEMSLEEP=%d,%d,%d", p1, *p2, *p3);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xnettime(struct ctr_lte_v2_talk *talk, int p1, int *p2)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2) {
		DIALOG_SEND_LINE("AT%%XNETTIME=%d", p1);
	} else {
		DIALOG_SEND_LINE("AT%%XNETTIME=%d,%d", p1, *p2);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xpofwarn(struct ctr_lte_v2_talk *talk, int p1, int p2)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XPOFWARN=%d,%d", p1, p2);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xrecvfrom(struct ctr_lte_v2_talk *talk, int p1, char *buf, size_t size,
				 size_t *len)
{
	DIALOG_PROLOG /* clang-format off */

	char xrecvfrom[32] = {0};

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XRECVFROM=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_PFX("#XRECVFROM: ", xrecvfrom, sizeof(xrecvfrom));
	});
	char *p = strchr(xrecvfrom, ',');
	if (!p) {
		DIALOG_ABORT(-EPIPE);
	} else {
		*p = '\0';
	}
	if (strlen(xrecvfrom) > 4) {
		DIALOG_ABORT(-EPIPE);
	}
	for (size_t i = 0; i < strlen(xrecvfrom); i++) {
		if (!isdigit((int)xrecvfrom[i])) {
			DIALOG_ABORT(-EPIPE);
		}
	}
	long len_ = strtol(xrecvfrom, NULL, 10);
	if (len_ > size) {
		DIALOG_ABORT(-ENOSPC);
	}
	DIALOG_RECV_DATA(RESPONSE_TIMEOUT_S, buf, len_, len);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsendto(struct ctr_lte_v2_talk *talk, const char *p1, int p2,
			       const void *buf, size_t len)
{
	DIALOG_PROLOG /* clang-format off */

	char xsendto[16] = {0};

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XSENDTO=\"%s\",%d", p1, p2);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_SEND_DATA(buf, len);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_PFX("#XDATAMODE: ", xsendto, sizeof(xsendto));
	});
	DIALOG_SEND_DATA("+++", 3);
	if (!strlen(xsendto)) {
		DIALOG_ABORT(-EPIPE);
	}
	for (size_t i = 0; i < strlen(xsendto); i++) {
		if (!isdigit((int)xsendto[i])) {
			DIALOG_ABORT(-EPIPE);
		}
	}
	if (len != strtol(xsendto, NULL, 10)) {
		DIALOG_ABORT(-EPIPE);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("#XDATAMODE: 0");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xrecv(struct ctr_lte_v2_talk *talk, int timeout, char *buf, size_t size,
			     size_t *len)
{
	DIALOG_PROLOG /* clang-format off */

	char xrecv[16] = {0};

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XRECV=%d", timeout);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_PFX("#XRECV: ", xrecv, sizeof(xrecv));
	});
	if (!strlen(xrecv)) {
		DIALOG_ABORT(-EPIPE);
	}
	for (size_t i = 0; i < strlen(xrecv); i++) {
		if (!isdigit((int)xrecv[i])) {
			DIALOG_ABORT(-EPIPE);
		}
	}
	long len_ = strtol(xrecv, NULL, 10);
	if (len_ > size) {
		DIALOG_ABORT(-ENOSPC);
	}
	DIALOG_RECV_DATA(RESPONSE_TIMEOUT_S, buf, len_, len);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsend(struct ctr_lte_v2_talk *talk, const void *buf, size_t len)
{
	DIALOG_PROLOG /* clang-format off */

	char xsend[16] = {0};

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XSEND=");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_SEND_DATA(buf, len);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_PFX("#XDATAMODE: ", xsend, sizeof(xsend));
	});
	DIALOG_SEND_DATA("+++", 3);
	if (!strlen(xsend)) {
		DIALOG_ABORT(-EPIPE);
	}
	for (size_t i = 0; i < strlen(xsend); i++) {
		if (!isdigit((int)xsend[i])) {
			DIALOG_ABORT(-EPIPE);
		}
	}
	if (len != strtol(xsend, NULL, 10)) {
		DIALOG_ABORT(-EPIPE);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("#XDATAMODE: 0");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsim(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XSIM=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsleep(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XSLEEP=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xslmver(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XSLMVER");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("#XSLMVER: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsocket(struct ctr_lte_v2_talk *talk, int p1, int *p2, int *p3, char *buf,
			       size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p2 && !p3) {
		DIALOG_SEND_LINE("AT#XSOCKET=%d", p1);
	} else if (p2 && p3) {
		DIALOG_SEND_LINE("AT#XSOCKET=%d,%d,%d", p1, *p2, *p3);
	} else {
		DIALOG_ABORT(-EINVAL);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("#XSOCKET: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsocketopt(struct ctr_lte_v2_talk *talk, int p1, int p2, int *p3)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	if (!p3) {
		DIALOG_SEND_LINE("AT#XSOCKETOPT=%d,%d", p1, p2);
	} else {
		DIALOG_SEND_LINE("AT#XSOCKETOPT=%d,%d,%d", p1, p2, *p3);
	}
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xsystemmode(struct ctr_lte_v2_talk *talk, int p1, int p2, int p3, int p4)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XSYSTEMMODE=%d,%d,%d,%d", p1, p2, p3, p4);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xtemp(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XTEMP=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xtemphighlvl(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XTEMPHIGHLVL=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xtime(struct ctr_lte_v2_talk *talk, int p1)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XTIME=%d", p1);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xversion(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT#XVERSION");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("#XVERSION: ", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_xmodemtrace(struct ctr_lte_v2_talk *talk)
{

	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT%%XMODEMTRACE=1,2");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at(struct ctr_lte_v2_talk *talk)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_crsm_176(struct ctr_lte_v2_talk *talk, char *buf, size_t size)
{
	if (size < 24 + 2 + 1) {
		return -ENOBUFS;
	}

	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CRSM=176,28539,0,0,12");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("+CRSM: 144,0,", buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_crsm_214(struct ctr_lte_v2_talk *talk)
{
	DIALOG_PROLOG /* clang-format off */

	char buf[4] = {0};

	DIALOG_ENTER();
	DIALOG_SEND_LINE("AT+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"");
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX("+CRSM: 144,0,", buf, sizeof(buf));
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	if (strcmp(buf, "\"\"")) {
		return -EINVAL;
	}

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cmd(struct ctr_lte_v2_talk *talk, const char *s)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("%s", s);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		DIALOG_LOOP_BREAK_ON_STR("OK");
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cmd_with_resp(struct ctr_lte_v2_talk *talk, const char *s, char *buf,
				     size_t size)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("%s", s);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER(buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}

int ctr_lte_v2_talk_at_cmd_with_resp_prefix(struct ctr_lte_v2_talk *talk, const char *s, char *buf,
					    size_t size, const char *pfx)
{
	DIALOG_PROLOG /* clang-format off */

	DIALOG_ENTER();
	DIALOG_SEND_LINE("%s", s);
	DIALOG_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LOOP_ABORT_ON_PFX("ERROR");
		if (!DIALOG_LOOP_GATHER_GET_COUNT()) {
			DIALOG_LOOP_GATHER_PFX(pfx, buf, size);
		} else {
			DIALOG_LOOP_BREAK_ON_STR("OK");
		}
	});
	DIALOG_EXIT();

	DIALOG_EPILOG /* clang-format on */
}
