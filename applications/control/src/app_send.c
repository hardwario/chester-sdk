#include "app_backup.h"
#include "app_cbor.h"
#include "app_codec.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

int app_send(void)
{
	int ret;

	LOG_WRN("Running UPLOAD DATA");

	CTR_BUF_DEFINE_STATIC(buf, 4096);

	ctr_buf_reset(&buf);

	ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

	ret = app_cbor_encode(zs);
	if (ret) {
		LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
		return ret;
	}

	size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

	ret = ctr_buf_seek(&buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
	if (ret) {
		LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
		return ret;
	}

	LOG_WRN("Stopped UPLOAD DATA");

	return 0;
}

static int cmd_poll(const struct shell *shell, size_t argc, char **argv)
{
	ctr_cloud_poll_immediately();

	return 0;
}

SHELL_CMD_REGISTER(poll, NULL, "Poll cloud for new data immediately.", cmd_poll);
