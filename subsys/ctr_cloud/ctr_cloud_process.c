/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_process.h"
#include "ctr_cloud_util.h"
#include "ctr_cloud_msg.h"
#include "ctr_cloud_transfer.h"

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>

/* NCS includes */
#include <dfu/dfu_target.h>
#include <dfu/dfu_target_mcuboot.h>

/* CHESTER includes */
#include <chester/ctr_config.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>

LOG_MODULE_REGISTER(ctr_cloud_process, CONFIG_CTR_CLOUD_LOG_LEVEL);

static bool is_ignored_config_cmd(const char *cmd)
{
	static const char *ignored_cmds[] = {
		"config save",
		"config reset",
		"config show",
	};

	for (size_t i = 0; i < ARRAY_SIZE(ignored_cmds); i++) {
		if (strcmp(cmd, ignored_cmds[i]) == 0) {
			return true;
		}
	}

	return false;
}

int ctr_cloud_process_dlconfig(struct ctr_cloud_msg_dlconfig *config)
{
	LOG_INF("Received config: num lines: %d", config->lines);

	CTR_BUF_DEFINE(line, CONFIG_SHELL_CMD_BUFF_SIZE);

	int ret;

	const struct shell *sh = shell_backend_dummy_get_ptr();

	for (int i = 0; i < config->lines; i++) {
		ctr_buf_reset(&line);

		ret = ctr_cloud_msg_dlconfig_get_next_line(config, &line);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_dlconfig_get_next_line` failed: %d", ret);
			return ret;
		}

		const char *cmd = ctr_buf_get_mem(&line);

		LOG_INF("Command %d: %s", i, cmd);

		if (is_ignored_config_cmd(cmd)) {
			LOG_WRN("Ignoring command: %s", cmd);
			continue;
		}

		shell_backend_dummy_clear_output(sh);

		ret = shell_execute_cmd(sh, cmd);

		if (ret) {
			LOG_ERR("Failed to execute shell command: %s", cmd);
			return ret;
		}

		size_t size;

		const char *p = shell_backend_dummy_get_output(sh, &size);
		if (!p) {
			LOG_ERR("Failed to get shell output");
			return -ENOMEM;
		}

		LOG_INF("Shell output: %s", p);

		k_sleep(K_MSEC(10));
	}

	k_sleep(K_SECONDS(1));

	LOG_INF("Save config and reboot");

#if defined CONFIG_ZTEST
	return 0;
#endif

	k_sleep(K_SECONDS(1));

	ret = ctr_config_save(true); // Save config and reboot
	if (ret) {
		LOG_ERR("Call `ctr_config_save` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_process_dlshell(struct ctr_cloud_msg_dlshell *dlshell, struct ctr_buf *buf)
{
	const struct shell *sh = shell_backend_dummy_get_ptr();

	shell_backend_dummy_clear_output(sh);

	LOG_INF("Received shell: num cmds: %d", dlshell->commands);

	int ret;
	size_t size;
	struct ctr_cloud_msg_upshell upshell;

	CTR_BUF_DEFINE(command, CONFIG_SHELL_CMD_BUFF_SIZE);

	ctr_cloud_msg_pack_upshell_start(&upshell, buf, dlshell->message_id);

	for (int i = 0; i < dlshell->commands; i++) {
		ctr_buf_reset(&command);
		ret = ctr_cloud_msg_dlshell_get_next_command(dlshell, &command);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_dlshell_get_next_command` failed: %d", ret);
			return ret;
		}

		shell_backend_dummy_clear_output(sh);
		const char *cmd = ctr_buf_get_mem(&command);

		LOG_DBG("Execute command %d: %s", i, cmd);

		int result = shell_execute_cmd(sh, cmd);

		const char *output = (char *)shell_backend_dummy_get_output(sh, &size);
		if (!output) {
			LOG_ERR("Failed to get output");
			return -ENOMEM;
		}

		LOG_DBG("Shell output: %s", output);

		ret = ctr_cloud_msg_pack_upshell_add_response(&upshell, cmd, result, output);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_pack_upshell_add_cmd` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_cloud_msg_pack_upshell_end(&upshell);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_pack_upshell_end` failed: %d", ret);
		return ret;
	}

	return 0;
}

#if CONFIG_DFU_TARGET_MCUBOOT
static void dfu_target_callback_handler(enum dfu_target_evt_id evt)
{
	switch (evt) {
	case DFU_TARGET_EVT_TIMEOUT:
		LOG_INF("DFU_TARGET_EVT_TIMEOUT");
		break;
	case DFU_TARGET_EVT_ERASE_DONE:
		LOG_INF("DFU_TARGET_EVT_ERASE_DONE");
		break;
	default:
		LOG_INF("Unknown event");
		break;
	}
}
#endif

int ctr_cloud_process_dlfirmware(struct ctr_cloud_msg_dlfirmware *dlfirmware, struct ctr_buf *buf)
{
	int ret;

	LOG_INF("Received firmware: target: %s, type: %s, offset: %d, length: %d",
		dlfirmware->target, dlfirmware->type, dlfirmware->offset, dlfirmware->length);
	if (dlfirmware->firmware_size) {
		LOG_INF("Firmware size: %d", dlfirmware->firmware_size);
	}

	if (strcmp(dlfirmware->target, "app") != 0) {
		LOG_ERR("Unsupported target: %s", dlfirmware->target);
		return -EINVAL;
	}

	size_t offset = 0;

	if (strcmp(dlfirmware->type, "chunk") == 0) {
		if (dlfirmware->firmware_size == 0) {
			LOG_ERR("Firmware size is 0");
			return -EINVAL;
		}

#if CONFIG_DFU_TARGET_MCUBOOT
		static uint8_t mcuboot_buf[256] __aligned(4);

		ret = dfu_target_mcuboot_set_buf(mcuboot_buf, sizeof(mcuboot_buf));
		if (ret) {
			LOG_ERR("dfu_target_mcuboot_set_buf failed: %d", ret);
			return ret;
		}

		if (dlfirmware->offset == 0) {
			dfu_target_reset();

			ret = dfu_target_init(DFU_TARGET_IMAGE_TYPE_MCUBOOT, 0,
					      dlfirmware->firmware_size,
					      dfu_target_callback_handler);
			if (ret) {
				LOG_ERR("dfu_target_init failed: %d", ret);

				struct ctr_cloud_upfirmware upfirmware = {
					.target = "app",
					.type = "error",
					.id = dlfirmware->id,
					.offset = offset,
					.error = "image size too big"};

				ret = ctr_cloud_msg_pack_firmware(buf, &upfirmware);
				if (ret) {
					LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d", ret);
					return ret;
				}

				return ret;
			}
		} else {
			ret = dfu_target_offset_get(&offset);
			if (ret) {
				LOG_ERR("dfu_target_offset_get failed: %d", ret);

				if (ret == -EACCES) {
					struct ctr_cloud_upfirmware upfirmware = {
						.target = "app",
						.type = "error",
						.id = dlfirmware->id,
						.offset = offset,
						.error = "offset mismatch (device was rebooted)"};

					ret = ctr_cloud_msg_pack_firmware(buf, &upfirmware);
					if (ret) {
						LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d",
							ret);
						return ret;
					}

					return 0;
				}

				return ret;
			}
		}
#else
		LOG_ERR("Unsupported MCUBOOT: %s", dlfirmware->type);

		struct ctr_cloud_upfirmware upfirmware = {.target = "app",
							  .type = "error",
							  .id = dlfirmware->id,
							  .offset = offset,
							  .error = "unsupported MCUBOOT"};

		ret = ctr_cloud_msg_pack_firmware(buf, &upfirmware);
		if (ret) {
			LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d", ret);
			return ret;
		}

		return 0;
#endif

	} else {
		LOG_ERR("Unsupported type: %s", dlfirmware->type);
		return -EINVAL;
	}

	if (offset != dlfirmware->offset) {
		LOG_ERR("Invalid offset: %d, expected: %d", offset, dlfirmware->offset);
		return -EINVAL;
	}

	ret = dfu_target_write(dlfirmware->data, dlfirmware->length);
	if (ret) {
		LOG_ERR("dfu_target_write failed: %d", ret);
		return ret;
	}

	ret = dfu_target_offset_get(&offset);
	if (ret) {
		LOG_ERR("dfu_target_offset_get failed: %d", ret);
		return ret;
	}

	if (offset == dlfirmware->firmware_size) {
		ret = dfu_target_done(true);
		if (ret) {
			LOG_ERR("dfu_target_done failed: %d", ret);
			return ret;
		}

		ret = dfu_target_schedule_update(0);
		if (ret) {
			LOG_ERR("dfu_target_schedule_update failed: %d", ret);
			return ret;
		}

		LOG_INF("Firmware update scheduled");

		struct ctr_cloud_upfirmware upfirmware = {
			.target = "app",
			.type = "swap",
			.id = dlfirmware->id,
			.offset = offset,
		};

		ret = ctr_cloud_msg_pack_firmware(buf, &upfirmware);
		if (ret) {
			LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d", ret);
			return ret;
		}

		ret = ctr_cloud_transfer_uplink(buf, NULL, K_FOREVER);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_transfer_uplink` for upbuf failed: %d", ret);
			return ret;
		}

		ctr_cloud_util_save_firmware_update_id(upfirmware.id);

		LOG_INF("Reboot to apply firmware update");

#if defined CONFIG_ZTEST
		return 0;
#endif
		k_sleep(K_MSEC(100));

		sys_reboot(SYS_REBOOT_COLD);

	} else {
		LOG_DBG("Firmware next offset: %d", offset);

		struct ctr_cloud_upfirmware upfirmware = {
			.target = "app",
			.type = "next",
			.id = dlfirmware->id,
			.offset = offset,
			.max_length = ((CTR_CLOUD_TRANSFER_BUF_SIZE - 50) / 256) * 256,
		};

		ret = ctr_cloud_msg_pack_firmware(buf, &upfirmware);
		if (ret) {
			LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d", ret);
			return ret;
		}

		LOG_DBG("Send next firmware");
	}

	return 0;
}
