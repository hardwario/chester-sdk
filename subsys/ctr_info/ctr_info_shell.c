/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_info.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_info_shell, CONFIG_CTR_INFO_LOG_LEVEL);

static int cmd_vendor_name(const struct shell *shell, size_t argc, char **argv)
{
	char *vendor_name;
	ctr_info_get_vendor_name(&vendor_name);
	shell_print(shell, "vendor name: %s", vendor_name);

	return 0;
}

static int cmd_product_name(const struct shell *shell, size_t argc, char **argv)
{
	char *product_name;
	ctr_info_get_product_name(&product_name);
	shell_print(shell, "product name: %s", product_name);

	return 0;
}

static int cmd_hw_variant(const struct shell *shell, size_t argc, char **argv)
{
	char *hw_variant;
	ctr_info_get_hw_variant(&hw_variant);
	shell_print(shell, "hardware variant: %s", hw_variant);

	return 0;
}

static int cmd_hw_revision(const struct shell *shell, size_t argc, char **argv)
{
	char *hw_revision;
	ctr_info_get_hw_revision(&hw_revision);
	shell_print(shell, "hardware revision: %s", hw_revision);

	return 0;
}

static int cmd_fw_name(const struct shell *shell, size_t argc, char **argv)
{
	char *fw_name;
	ctr_info_get_fw_name(&fw_name);
	shell_print(shell, "firmware name: %s", fw_name);

	return 0;
}

static int cmd_fw_version(const struct shell *shell, size_t argc, char **argv)
{
	char *fw_version;
	ctr_info_get_fw_version(&fw_version);
	shell_print(shell, "firmware version: %s", fw_version);

	return 0;
}

static int cmd_serial_number(const struct shell *shell, size_t argc, char **argv)
{
	char *serial_number;
	ctr_info_get_serial_number(&serial_number);
	shell_print(shell, "serial number: %s", serial_number);

	return 0;
}

static int cmd_claim_token(const struct shell *shell, size_t argc, char **argv)
{
	char *claim_token;
	ctr_info_get_claim_token(&claim_token);
	shell_print(shell, "claim token: %s", claim_token);

	return 0;
}

static int cmd_ble_devaddr(const struct shell *shell, size_t argc, char **argv)
{
	char *ble_devaddr;
	ctr_info_get_ble_devaddr(&ble_devaddr);
	shell_print(shell, "ble devaddr: %s", ble_devaddr);

	return 0;
}

static int cmd_ble_passkey(const struct shell *shell, size_t argc, char **argv)
{
	char *ble_passkey;
	ctr_info_get_ble_passkey(&ble_passkey);
	shell_print(shell, "ble passkey: %s", ble_passkey);

	return 0;
}

static int cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	cmd_vendor_name(shell, argc, argv);
	cmd_product_name(shell, argc, argv);
	cmd_hw_variant(shell, argc, argv);
	cmd_hw_revision(shell, argc, argv);
	cmd_fw_name(shell, argc, argv);
	cmd_fw_version(shell, argc, argv);
	cmd_serial_number(shell, argc, argv);
	cmd_claim_token(shell, argc, argv);
	cmd_ble_devaddr(shell, argc, argv);
	cmd_ble_passkey(shell, argc, argv);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_info,

	SHELL_CMD_ARG(show, NULL,
	              "Get all information at once.",
	              cmd_show, 1, 0),

	SHELL_CMD_ARG(vendor-name, NULL,
	              "Get vendor name.",
	              cmd_vendor_name, 1, 0),

	SHELL_CMD_ARG(product-name, NULL,
	              "Get product name.",
	              cmd_product_name, 1, 0),

	SHELL_CMD_ARG(hw-variant, NULL,
	              "Get hardware variant.",
	              cmd_hw_variant, 1, 0),

	SHELL_CMD_ARG(hw-revision, NULL,
	              "Get hardware revision.",
	              cmd_hw_revision, 1, 0),

	SHELL_CMD_ARG(fw-name, NULL,
	              "Get firmware name.",
	              cmd_fw_name, 1, 0),

	SHELL_CMD_ARG(fw-version, NULL,
	              "Get firmware version.",
	              cmd_fw_version, 1, 0),

	SHELL_CMD_ARG(serial-number, NULL,
	              "Get serial number.",
	              cmd_serial_number, 1, 0),

	SHELL_CMD_ARG(claim-token, NULL,
	              "Get claim token.",
	              cmd_claim_token, 1, 0),

	SHELL_CMD_ARG(ble-devaddr, NULL,
	              "Get BLE device address.",
	              cmd_ble_devaddr, 1, 0),

	SHELL_CMD_ARG(ble-passkey, NULL,
	              "Get BLE passkey.",
	              cmd_ble_passkey, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(info, &sub_info, "Device information commands.", NULL);

/* clang-format on */
