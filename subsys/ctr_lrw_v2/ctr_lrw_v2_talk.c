/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lrw_v2_talk.h"

#include <chester/ctr_util.h>
#include <chester/drivers/ctr_lrw_link.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lrw_v2_talk, CONFIG_CTR_LRW_V2_LOG_LEVEL);

#define SEND_GUARD_TIME    K_MSEC(50)
#define SEND_TIMEOUT       K_SECONDS(1)
#define BAND_CHANGE_TIME   K_MSEC(5000)
#define RESPONSE_TIMEOUT_S K_MSEC(1000)
#define RESPONSE_TIMEOUT_L K_SECONDS(10)

#define DIALOG_LRW_PROLOG bool in_dialog = false;

#define DIALOG_LRW_EPILOG return 0;

#define DIALOG_LRW_ENTER()                                                                         \
	do {                                                                                       \
		int ret = ctr_lrw_link_enter_dialog(talk->dev);                                    \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lrw_link_enter_dialog` failed: %d", ret);               \
			return ret;                                                                \
		}                                                                                  \
		in_dialog = true;                                                                  \
	} while (0)

#define DIALOG_LRW_EXIT()                                                                          \
	do {                                                                                       \
		int ret = ctr_lrw_link_exit_dialog(talk->dev);                                     \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lrw_link_exit_dialog` failed: %d", ret);                \
			return ret;                                                                \
		}                                                                                  \
		in_dialog = false;                                                                 \
	} while (0)

#define DIALOG_LRW_ABORT(code)                                                                     \
	do {                                                                                       \
		if (in_dialog) {                                                                   \
			int ret = ctr_lrw_link_exit_dialog(talk->dev);                             \
			if (ret) {                                                                 \
				LOG_ERR("Call `ctr_lrw_link_exit_dialog` failed: %d", ret);        \
			}                                                                          \
		}                                                                                  \
		return code;                                                                       \
	} while (0)

#define DIALOG_LRW_SEND_LINE(...)                                                                  \
	do {                                                                                       \
		k_sleep(SEND_GUARD_TIME);                                                          \
		int ret = ctr_lrw_link_send_line(talk->dev, SEND_TIMEOUT, __VA_ARGS__);            \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lrw_link_send_line` failed: %d", ret);                  \
			DIALOG_LRW_ABORT(ret);                                                     \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_RECV_LINE(timeout, line)                                                        \
	do {                                                                                       \
		int ret = ctr_lrw_link_recv_line(talk->dev, timeout, line);                        \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lrw_link_recv_line` failed: %d", ret);                  \
			DIALOG_LRW_ABORT(ret);                                                     \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_FREE_LINE(line, abort)                                                          \
	do {                                                                                       \
		int ret = ctr_lrw_link_free_line(talk->dev, line);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_lrw_link_free_line` failed: %d", ret);                  \
			if (abort) {                                                               \
				DIALOG_LRW_ABORT(ret);                                             \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_LOOP_RUN(timeout, ...)                                                          \
	do {                                                                                       \
		__unused int gather_idx = 0;                                                       \
		k_timepoint_t end = sys_timepoint_calc(timeout);                                   \
		bool loop_run = true;                                                              \
		while (loop_run) {                                                                 \
			if (sys_timepoint_expired(end)) {                                          \
				DIALOG_LRW_ABORT(-ETIMEDOUT);                                      \
			}                                                                          \
			char *line;                                                                \
			DIALOG_LRW_RECV_LINE(timeout, &line);                                      \
			if (!line) {                                                               \
				continue;                                                          \
			}                                                                          \
			bool process_urc = true;                                                   \
			__VA_ARGS__                                                                \
			if (process_urc && talk->user_cb) {                                        \
				talk->user_cb(talk, line, talk->user_data);                        \
			}                                                                          \
			DIALOG_LRW_FREE_LINE(line, true);                                          \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_LOOP_BREAK_ON_STR(str)                                                          \
	do {                                                                                       \
		if (!strcmp(line, str)) {                                                          \
			loop_run = false;                                                          \
			process_urc = false;                                                       \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_LOOP_ABORT_ON_PFX(pfx)                                                          \
	do {                                                                                       \
		if (!strncmp(line, pfx, strlen(pfx))) {                                            \
			DIALOG_LRW_FREE_LINE(line, false);                                         \
			DIALOG_LRW_ABORT(-EILSEQ);                                                 \
		}                                                                                  \
	} while (0)

#define DIALOG_LRW_LOOP_GATHER_GET_COUNT() gather_idx

#define DIALOG_LRW_LOOP_GATHER(buf, size)                                                          \
	do {                                                                                       \
		if (buf[0] == '+' || buf[0] == '%' || buf[0] == = '#') {                           \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(line) >= size) {                                                \
				DIALOG_LRW_FREE_LINE(line, false);                                 \
				DIALOG_LRW_ABORT(-ENOSPC);                                         \
			} else {                                                                   \
				strcpy(buf, line);                                                 \
			}                                                                          \
		}                                                                                  \
		gather_idx++;                                                                      \
		process_urc = false;                                                               \
	} while (0)

#define DIALOG_LRW_LOOP_GATHER_PFX(pfx, buf, size)                                                 \
	do {                                                                                       \
		if (strncmp(line, pfx, strlen(pfx))) {                                             \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(&line[strlen(pfx)]) >= size) {                                  \
				DIALOG_LRW_FREE_LINE(line, false);                                 \
				DIALOG_LRW_ABORT(-ENOSPC);                                         \
			} else {                                                                   \
				strcpy(buf, &line[strlen(pfx)]);                                   \
				gather_idx++;                                                      \
				process_urc = false;                                               \
			}                                                                          \
		}                                                                                  \
		gather_idx++;                                                                      \
		process_urc = false;                                                               \
	} while (0)

#define DIALOG_LRW_BREAK_ON_PFX(pfx, buf, size)                                                    \
	do {                                                                                       \
		if (strncmp(line, pfx, strlen(pfx))) {                                             \
			break;                                                                     \
		}                                                                                  \
		if (size) {                                                                        \
			if (strlen(&line[strlen(pfx)]) >= size) {                                  \
				DIALOG_LRW_FREE_LINE(line, false);                                 \
				DIALOG_LRW_ABORT(-ENOSPC);                                         \
			} else {                                                                   \
				strcpy(buf, &line[strlen(pfx)]);                                   \
			}                                                                          \
		}                                                                                  \
		loop_run = false;                                                                  \
		process_urc = false;                                                               \
	} while (0)

int ctr_lrw_v2_talk_init(struct ctr_lrw_v2_talk *talk, const struct device *dev)
{
	talk->dev = dev;
	return 0;
}

int ctr_lrw_v2_talk_set_callback(struct ctr_lrw_v2_talk *talk, ctr_lrw_v2_talk_cb user_cb,
				 void *user_data)
{
	talk->user_cb = user_cb;
	talk->user_data = user_data;

	return 0;
}

int ctr_lrw_v2_talk_(struct ctr_lrw_v2_talk *talk, const char *s)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("%s", s);
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at(struct ctr_lrw_v2_talk *talk)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT");
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});

	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_dformat(struct ctr_lrw_v2_talk *talk, uint8_t df)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+DFORMAT=%u", df);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_band(struct ctr_lrw_v2_talk *talk, uint8_t band)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+BAND=%u", band);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_class(struct ctr_lrw_v2_talk *talk, uint8_t class)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+CLASS=%u", class);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_dr(struct ctr_lrw_v2_talk *talk, uint8_t dr)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+DR=%u", dr);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_mode(struct ctr_lrw_v2_talk *talk, uint8_t mode)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+MODE=%u", mode);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_nwk(struct ctr_lrw_v2_talk *talk, uint8_t network)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+NWK=%u", network);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_adr(struct ctr_lrw_v2_talk *talk, uint8_t adr)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+ADR=%u", adr);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR=");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_dutycycle(struct ctr_lrw_v2_talk *talk, uint8_t dc)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+DUTYCYCLE=%u", dc);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_async(struct ctr_lrw_v2_talk *talk, bool async)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT$ASYNC=%u", async);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_joindc(struct ctr_lrw_v2_talk *talk, uint8_t jdc)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+JOINDC=%u", jdc);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_join(struct ctr_lrw_v2_talk *talk)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+JOIN");
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_devaddr(struct ctr_lrw_v2_talk *talk, const uint8_t *devaddr,
			       size_t devaddr_size)
{
	int ret;

	size_t hex_size = devaddr_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(devaddr, devaddr_size, hex, hex_size, true);
	if (ret != devaddr_size * 2) {
		LOG_ERR("Failed to convert devaddr to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+DEVADDR=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_deveui(struct ctr_lrw_v2_talk *talk, const uint8_t *deveui,
			      size_t deveui_size)
{
	int ret;

	size_t hex_size = deveui_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(deveui, deveui_size, hex, hex_size, true);
	if (ret != deveui_size * 2) {
		LOG_ERR("Failed to convert deveui to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+DEVEUI=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_appeui(struct ctr_lrw_v2_talk *talk, const uint8_t *appeui,
			      size_t appeui_size)
{
	int ret;

	size_t hex_size = appeui_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(appeui, appeui_size, hex, hex_size, true);
	if (ret != appeui_size * 2) {
		LOG_ERR("Failed to convert appeui to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+APPEUI=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_appkey(struct ctr_lrw_v2_talk *talk, const uint8_t *appkey,
			      size_t appkey_size)
{
	int ret;

	size_t hex_size = appkey_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(appkey, appkey_size, hex, hex_size, true);
	if (ret != appkey_size * 2) {
		LOG_ERR("Failed to convert appkey to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+APPKEY=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_nwkskey(struct ctr_lrw_v2_talk *talk, const uint8_t *nwkskey,
			       size_t nwkskey_size)
{
	int ret;

	size_t hex_size = nwkskey_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(nwkskey, nwkskey_size, hex, hex_size, true);
	if (ret != nwkskey_size * 2) {
		LOG_ERR("Failed to convert nwkskey to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+NWKSKEY=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_appskey(struct ctr_lrw_v2_talk *talk, const uint8_t *appskey,
			       size_t appskey_size)
{
	int ret;

	size_t hex_size = appskey_size * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(appskey, appskey_size, hex, hex_size, true);
	if (ret != appskey_size * 2) {
		LOG_ERR("Failed to convert appskey to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+APPSKEY=%s", hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_chmask(struct ctr_lrw_v2_talk *talk, const char *chmask)
{
	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+CHMASK=%s", chmask);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_S, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_utx(struct ctr_lrw_v2_talk *talk, const void *payload, size_t payload_len)
{
	if (payload_len < 1 || payload_len > 242) {
		LOG_ERR("Payload length must be between 1 and 242");
		return -EINVAL;
	}

	int ret;

	size_t hex_size = payload_len * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);
	if (ret != payload_len * 2) {
		LOG_ERR("Failed to convert payload to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+UTX %u\r%s", payload_len, hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_ctx(struct ctr_lrw_v2_talk *talk, const void *payload, size_t payload_len)
{
	if (payload_len < 1 || payload_len > 242) {
		LOG_ERR("Payload length must be between 1 and 242");
		return -EINVAL;
	}

	int ret;

	size_t hex_size = payload_len * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);
	if (ret != payload_len * 2) {
		LOG_ERR("Failed to convert payload to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+CTX %u\r%s", payload_len, hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_putx(struct ctr_lrw_v2_talk *talk, uint8_t port, const void *payload,
			    size_t payload_len)
{
	if (payload_len < 1 || payload_len > 242) {
		LOG_ERR("Payload length must be between 1 and 242");
		return -EINVAL;
	}

	int ret;

	size_t hex_size = payload_len * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);
	if (ret != payload_len * 2) {
		LOG_ERR("Failed to convert payload to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+PUTX %u,%u\r%s", port, payload_len, hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}

int ctr_lrw_v2_talk_at_pctx(struct ctr_lrw_v2_talk *talk, uint8_t port, const void *payload,
			    size_t payload_len)
{
	if (payload_len < 1 || payload_len > 242) {
		LOG_ERR("Payload length must be between 1 and 242");
		return -EINVAL;
	}

	int ret;

	size_t hex_size = payload_len * 2 + 1;
	char hex[hex_size];

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);
	if (ret != payload_len * 2) {
		LOG_ERR("Failed to convert payload to hex string");
		k_free(hex);
		return ret;
	}

	DIALOG_LRW_PROLOG /* clang-format off */

	DIALOG_LRW_ENTER();
	DIALOG_LRW_SEND_LINE("AT+PCTX %u,%u\r%s", port, payload_len, hex);
	DIALOG_LRW_LOOP_RUN(RESPONSE_TIMEOUT_L, {
		DIALOG_LRW_LOOP_ABORT_ON_PFX("+ERR");
		DIALOG_LRW_LOOP_BREAK_ON_STR("+OK");
	});
	DIALOG_LRW_EXIT();

	DIALOG_LRW_EPILOG /* clang-format on */
}
