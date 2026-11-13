/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_INTERFACE_H_
#define DRV_INTERFACE_H_

/**
 * @file drv_interface.h
 * @brief Driver interface for device drivers
 *
 * This header is included only by device drivers (drv_microsens_180hs.c, etc.).
 * It defines the interface that each driver must implement.
 */

#include "../app_device.h"
#include "../app_driver_store.h"
#include "../app_modbus_decode.h"

#include <zephyr/shell/shell.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device driver interface
 *
 * Each device driver implements this interface. Every callback gets the slot of the
 * configured device (device-N), which carries the Modbus address and the per-slot
 * storage, so two devices of the same type never share state.
 *
 * Functions can be NULL if the driver doesn't support that operation.
 *
 * A table-driven Modbus driver only provides its item table: with .sample == NULL,
 * every item with a register is read with .fc and stored in the slot's item store.
 * A driver keeps either an item table or raw records (rec_size), not both: reports
 * encode one of them, a device whose driver has both is ignored.
 *
 * @code
 * static const struct app_device_driver my_driver = {
 *     .name = "mydevice",
 *     .type = APP_DEVICE_TYPE_MYDEVICE,
 *     .transport = APP_DEVICE_TRANSPORT_MODBUS,
 *     .ctx_size = sizeof(struct mydevice_ctx), // latest values, shown / sent over LoRaWAN
 *     .rec_size = sizeof(struct mydevice_sample), // one raw record per sample
 *     .sample = mydevice_sample,
 *     .config = mydevice_config,   // Optional
 *     .print_data = mydevice_print_data,
 * };
 * @endcode
 */
struct app_device_driver {
	/* Identification */
	const char *name;          /**< Driver name ("microsens", "sensecap", ...) */
	enum app_device_type type; /**< Device type enum value */

	/* Transport this device talks over - decides which of app_serial_init() /
	 * app_modbus_init() has to run for it, and is validated against the
	 * configured serial-mode. State it explicitly in every driver: the zero
	 * default (MODBUS) exists for the driverless legacy types, and a UART
	 * driver that forgets this field is silently put on the wrong bus.
	 */
	enum app_device_transport transport;

	/* Storage demand per slot */
	uint8_t max_instances; /**< Max configured slots of this type (0 = no limit) */
	size_t ctx_size;       /**< Driver private state (slot->ctx) */
	size_t rec_size;       /**< Raw record size (slot->rec), 0 = no records */
	uint16_t rec_fixed;    /**< Fixed record capacity, 0 = derived from the intervals */

	/* Item table (aggregated per interval-aggreg window, see app_driver_store.h) */
	const struct app_driver_item *items;
	size_t item_count;
	enum app_modbus_fc fc; /**< Function code of the generic item read */
	uint32_t codec_key;    /**< CODEC_KEY_E_DEVICES__<GROUP> of the item map */

	/* Callbacks, all optional */
	int (*init)(struct app_driver_slot *slot);   /**< Before the first sample; retried */
	int (*sample)(struct app_driver_slot *slot); /**< NULL with items: generic read */
	int (*deinit)(struct app_driver_slot *slot);
	int (*calibrate)(struct app_driver_slot *slot, enum app_device_calibration cal_type,
			 int value);
	int (*reset)(struct app_driver_slot *slot); /**< Factory reset */
	int (*config)(struct app_driver_slot *slot, uint8_t new_addr,
		      const char *parity_str); /**< Change device address (and parity) */
	void (*print_data)(const struct shell *shell, struct app_driver_slot *slot);
};

/**
 * @brief Get driver instance by type
 *
 * Used internally by app_device.c to find the appropriate driver.
 *
 * @param type Device type to look up
 * @return Pointer to driver structure, or NULL if not found
 */
const struct app_device_driver *app_device_find_driver(enum app_device_type type);

/**
 * @brief Read every item with a register from the item table into the slot's item store
 *
 * The default sampling of table-driven drivers; a custom .sample can call it and add
 * computed items with app_driver_items_put().
 *
 * @return 0 if at least one item was read, -EIO if all reads failed
 */
int app_device_sample_items(struct app_driver_slot *slot);

/**
 * @brief Sample a device at an address and print the result
 *
 * Runs on a temporary probe slot (shell commands addressing a device directly, also
 * before it is configured). Nothing is kept for upload and no configured slot is
 * touched.
 */
int app_device_probe_sample(const struct shell *shell, const struct app_device_driver *drv,
			    uint8_t addr);

/**
 * @brief Run an operation on the probe slot of a driver at an address
 *
 * For driver-specific shell commands (e.g. reading a service register set); op runs with
 * the probe slot locked.
 */
int app_device_probe_run(const struct shell *shell, const struct app_device_driver *drv,
			 uint8_t addr,
			 int (*op)(const struct shell *shell, struct app_driver_slot *slot));

/**
 * @brief Call the .config of a driver for the device at an address (probe slot)
 */
int app_device_probe_config(const struct shell *shell, const struct app_device_driver *drv,
			    uint8_t addr, uint8_t new_addr, const char *parity_str);

/** Register read into a float field of a driver structure, see app_device_read_regs() */
struct app_device_reg {
	uint16_t reg;
	enum app_modbus_format fmt;
	float div; /**< value = raw / div (ignored for float formats) */
	size_t offset;
	const char *name;
};

#define APP_DEVICE_REG(_type, _field, _reg, _fmt, _div)                                            \
	{                                                                                          \
		.reg = _reg,                                                                       \
		.fmt = _fmt,                                                                       \
		.div = _div,                                                                       \
		.offset = offsetof(_type, _field),                                                 \
		.name = #_field,                                                                   \
	}

/**
 * @brief Read a list of registers into float fields of dst
 *
 * The caller holds the Modbus bus (app_modbus_acquire()). A read that times out is sent once
 * more if every read before it succeeded, so one missed request does not fail the call. The
 * first read is not repeated, so a device that does not answer is not waited for twice. If
 * the first read or the repeat gets no reply, the call stops there, also without
 * stop_on_error.
 *
 * @param stop_on_error Stop at the first failed read; otherwise read all and fail at the end
 * @return 0 if all reads succeeded, -ETIMEDOUT if the device did not answer the first read
 *         or the repeat, otherwise a negative error code or a positive Modbus exception code
 */
int app_device_read_regs(uint8_t addr, enum app_modbus_fc fc, const struct app_device_reg *regs,
			 size_t count, void *dst, bool stop_on_error);

/**
 * @brief Print the heading of print_data: "[idx] <title> @ addr <addr>:"
 */
void app_device_print_header(const struct shell *shell, const struct app_driver_slot *slot,
			     const char *title);

/**
 * @brief Print the items of a table-driven device: last sample and last closed window
 */
void app_device_print_items(const struct shell *shell, struct app_driver_slot *slot,
			    const char *title);

/*
 * Driver declarations
 *
 * Each driver declares its driver structure here so app_device.c can reference it.
 * Add new driver declarations when implementing new device types.
 */

/* MicroSens 180-HS driver */
extern const struct app_device_driver microsens_driver;

/* Energy meter drivers */
extern const struct app_device_driver or_we_504_driver;
extern const struct app_device_driver em1xx_driver;
extern const struct app_device_driver or_we_516_driver;
extern const struct app_device_driver em5xx_driver;
extern const struct app_device_driver iem3000_driver;

/* Promag MF7S RFID reader driver */
extern const struct app_device_driver promag_mf7s_driver;

/* FlowT FT201 ultrasonic flowmeter driver */
extern const struct app_device_driver flowt_ft201_driver;

/* Piketronic RPP-R radon probe driver */
extern const struct app_device_driver piketronic_rpp_driver;

/* SolaX X3-Hybrid G3 inverter driver */
extern const struct app_device_driver solax_g3_driver;

/* Piketronic RPP-U radon probe driver (modified UART-only variant) */
extern const struct app_device_driver piketronic_rppu_driver;

/* SenseCAP S1000 / S700 / S500 weather station drivers */
extern const struct app_device_driver sensecap_s1000_driver;
extern const struct app_device_driver sensecap_s500_driver;
extern const struct app_device_driver sensecap_s700_driver;

/* Cubic 6303 particulate matter sensor driver */
extern const struct app_device_driver cubic_6303_driver;

/* Shell command entries */
extern const union shell_cmd_entry microsens_shell_cmds;
extern const union shell_cmd_entry or_we_504_shell_cmds;
extern const union shell_cmd_entry em1xx_shell_cmds;
extern const union shell_cmd_entry or_we_516_shell_cmds;
extern const union shell_cmd_entry em5xx_shell_cmds;
extern const union shell_cmd_entry iem3000_shell_cmds;
extern const union shell_cmd_entry promag_mf7s_shell_cmds;
extern const union shell_cmd_entry flowt_ft201_shell_cmds;
extern const union shell_cmd_entry piketronic_rpp_shell_cmds;
extern const union shell_cmd_entry solax_g3_shell_cmds;
extern const union shell_cmd_entry piketronic_rppu_shell_cmds;
extern const union shell_cmd_entry sensecap_shell_cmds;
extern const union shell_cmd_entry cubic_6303_shell_cmds;

#ifdef __cplusplus
}
#endif

#endif /* DRV_INTERFACE_H_ */
