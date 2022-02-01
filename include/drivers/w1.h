/*
 * Copyright (c) 2018 Roman Tataurov <diytronic@yandex.ru>
 * Copyright (c) 2021 Thomas Stranger
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public 1-Wire Driver APIs
 */

#ifndef CHESTER_INCLUDE_DRIVERS_W1_H_
#define CHESTER_INCLUDE_DRIVERS_W1_H_

#include <zephyr/types.h>
#include <sys/byteorder.h>
#include <sys/crc.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 1-Wire Interface
 * @defgroup w1_interface 1-Wire Interface
 * @ingroup io_interfaces
 * @{
 */

/**  @cond INTERNAL_HIDDEN */
typedef bool (*w1_reset_bus_t)(const struct device *dev);

typedef bool (*w1_read_bit_t)(const struct device *dev);

typedef void (*w1_write_bit_t)(const struct device *dev, const bool bit);

typedef uint8_t (*w1_read_byte_t)(const struct device *dev);

typedef void (*w1_write_byte_t)(const struct device *dev, const uint8_t byte);

typedef void (*w1_read_block_t)(const struct device *dev, uint8_t *buffer, size_t length);

typedef void (*w1_write_block_t)(const struct device *dev, const uint8_t *buffer, size_t length);

typedef void (*w1_lock_bus_t)(const struct device *dev);

typedef void (*w1_unlock_bus_t)(const struct device *dev);

typedef size_t (*w1_get_peripheral_count_t)(const struct device *dev);

struct w1_driver_api {
	w1_reset_bus_t reset_bus;
	w1_read_bit_t read_bit;
	w1_write_bit_t write_bit;
	w1_read_byte_t read_byte;
	w1_write_byte_t write_byte;
	w1_read_block_t read_block;
	w1_write_block_t write_block;
	w1_lock_bus_t lock_bus;
	w1_unlock_bus_t unlock_bus;
};
/** @endcond */

/**
 * @brief Lock the 1-wire bus to prevent simultaneous access.
 *
 * This routine locks the bus to prevent simultaneous access from different
 * threads. The calling thread waits until the bus becomes available.
 * A thread is permitted to lock a mutex it has already locked.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 */
static inline void w1_lock_bus(const struct device *dev)
{
	const struct w1_driver_api *api = dev->api;

	api->lock_bus(dev);
}

/**
 * @brief Unlock the 1-wire bus.
 *
 * This routine unlocks the bus to permit access to bus line.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 */
static inline void w1_unlock_bus(const struct device *dev)
{
	const struct w1_driver_api *api = dev->api;

	api->unlock_bus(dev);
}

/**
 * @brief 1-Wire data link layer
 * @defgroup w1_data_link 1-Wire data link layer
 * @ingroup w1_interface
 * @{
 */

/**
 * @brief Reset the 1-Wire bus to prepare peripheral devices for communication.
 *
 * This routine resets all 1-Wire bus peripheral devices such that they are
 * ready to receive a command.
 * Connected peripherals answer with a presence pulse once they are ready
 * to receive data.
 *
 * @param dev    Pointer to the device structure for the driver instance.
 *
 * @retval false If no devices answer with a present pulse.
 * @retval true  If at least one device answers with a present pulse.
 */
static inline bool w1_reset_bus(const struct device *dev)
{
	const struct w1_driver_api *api = dev->api;

	return api->reset_bus(dev);
}

/**
 * @brief Read a single bit from the 1-Wire device.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 *
 * @retval     Current bit value.
 */
static inline bool w1_read_bit(const struct device *dev)
{
	const struct w1_driver_api *api = dev->api;

	return api->read_bit(dev);
}

/**
 * @brief Write a single bit to the 1-Wire device.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 * @param bit  Transmitting bit value 1 or 0.
 */
static inline void w1_write_bit(const struct device *dev, bool bit)
{
	const struct w1_driver_api *api = dev->api;

	api->write_bit(dev, bit);
}

/**
 * @brief Read a single byte from the 1-Wire device.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 *
 * @retval     Read byte value.
 */
static inline uint8_t w1_read_byte(const struct device *dev)
{
	const struct w1_driver_api *api = dev->api;

	return api->read_byte(dev);
}

/**
 * @brief Write a single byte to the 1-Wire device.
 *
 * @param dev   Pointer to the device structure for the driver instance.
 * @param byte  Transmitting byte.
 */
static inline void w1_write_byte(const struct device *dev, uint8_t byte)
{
	const struct w1_driver_api *api = dev->api;

	api->write_byte(dev, byte);
}

/**
 * @brief Read a block of data from the 1-Wire device.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param buffer  Pointer to receive buffer.
 * @param length  Length of receiving buffer (in bytes).
 */
static inline void w1_read_block(const struct device *dev, uint8_t *buffer, size_t length)
{
	const struct w1_driver_api *api = dev->api;

	api->read_block(dev, buffer, length);
}

/**
 * @brief Write a block of data from the 1-Wire device.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param buffer  Pointer to transmitting buffer.
 * @param length  Length of transmitting buffer (in bytes).
 */
static inline void w1_write_block(const struct device *dev, const uint8_t *buffer, size_t length)
{
	const struct w1_driver_api *api = dev->api;

	api->write_block(dev, buffer, length);
}

/**
 * @}
 */

/**
 * @brief 1-Wire network layer
 * @defgroup w1_network 1-Wire network layer
 * @ingroup w1_interface
 * @{
 */

/*
 * ROM Commands
 */
#define W1_CMD_SKIP_ROM 0xCC
#define W1_CMD_MATCH_ROM 0x55
#define W1_CMD_RESUME 0xA5
#define W1_CMD_READ_ROM 0x33
#define W1_CMD_SEARCH_ROM 0xF0
#define W1_CMD_SEARCH_ALARM 0xEC
#define W1_CMD_OVERDRIVE_SKIP_ROM 0x3C
#define W1_CMD_OVERDRIVE_MATCH_ROM 0x69

/*
 * CRC definitions
 */
#define W1_CRC8_INITIAL_VALUE 0x00
/** polynomial used to calculate 8-bit crc */
#define W1_CRC8_POLYNOMIAL 0x8C

/** this flag can be passed to searches in order to not filter on family ID */
#define W1_SEARCH_ALL_FAMILIES 0x00

/** Intitialize a w1_rom struct zeroes */
#define W1_ROM_INIT_ZERO                                                                           \
	{                                                                                          \
		.family = 0, .serial = { 0 }, .crc = 0,                                            \
	}

/**
 * @brief w1_rom struct.
 */
struct w1_rom {
	/** 1-Wire family code identifies the device type
	 *
	 * Incomplete list of family codes is available at:
	 * https://www.maximintegrated.com/en/app-notes/index.mvp/id/155
	 * others are documented in the respective device data sheet.
	 */
	uint8_t family;
	/** Serial together with the family code composes the unique id */
	uint8_t serial[6];
	/** Checksum of 56-bit unique id */
	uint8_t crc;
};

/** holds configuration data common to all w1-controller implementations */
struct w1_controller_config {
	/* Number of connected client devices */
	uint16_t client_count;
};

/**
 * @typedef w1_search_callback_t
 * @brief Define the application callback handler function signature
 *        for searches.
 *
 * @param rom found The ROM of the found device.
 * @param cb_arg    The argument that was passed to the search command.
 */
typedef void (*w1_search_callback_t)(struct w1_rom rom, void *cb_arg);

/**
 * @brief Get the number of peripheral devices on the bus.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 *
 * @retval     Number of connected w1 peripheral devices.
 */
static inline size_t w1_get_peripheral_count(const struct device *dev)
{
	const struct w1_controller_config *ctrl_cfg = dev->config;

	return ctrl_cfg->client_count;
}

/**
 * @brief Read Peripheral 64-bit ROM ID.
 *
 * This procedure allows the bus master to read the peripherals’s 64-bit ROM id
 * without using the Search ROM procedure.
 * This command can be used as long as not more than a sigle peripheral is
 * connected to the bus.
 * Otherwise data collisons occur and a faulty ROM id is read.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param rom_id  Pointer to the ROM id structure.
 *
 * @retval 0 If successful.
 * @retval -EIO no device present, or invalid rom received.
 */
int w1_read_rom(const struct device *dev, struct w1_rom *rom_id);

/**
 * @brief Select a specific device by broadcasting a selected ROM ID.
 *
 * This routine allows the bus master to select a peripheral device identified
 * by it's unique ROM id, such that the next command will target only this
 * single selected device.
 *
 * This command is only necessary in multidrop environments, otherwise the
 * skip rom command can be issued instead.
 * Once a device has been selected, the resume command can be used instead of
 * this command to reduce the communication overhead.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param rom_id  Pointer to the ROM id structure.
 *
 * @retval 0 If successful.
 * @retval -EIO no device present.
 */
int w1_match_rom(const struct device *dev, const struct w1_rom *rom_id);

/**
 * @brief Select the device last addressed with a Match ROM or Search ROM commnad.
 *
 * This routine allows the bus master to re-select a peripheral device
 * which was already addressed using a Match ROm or Search ROM command.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 *
 * @retval 0 If successful.
 * @retval -EIO no device present.
 */
int w1_resume_command(const struct device *dev);

/**
 * @brief Select all devices regardless of ROM ID.
 *
 * This routine sets up the bus devices to receive a command.
 * It is usually used when there is only one peripheral on the bus
 * to avoid the overhead of the match rom command.
 * But it can also be used to concurrently write to all devices.
 *
 * @param dev  Pointer to the device structure for the driver instance.
 *
 * @retval 0 If successful.
 * @retval -EIO no device present.
 */
int w1_skip_rom(const struct device *dev);

/**
 * @brief In single drop configurations use SKIP_SELECT command, else use
 *        MATCH_ROM command.
 *
 * @retval 0 If successful.
 * @retval -EIO  If no device is connected to the bus.
 */
int w1_reset_select(const struct device *dev, const struct w1_rom *rom_id);

/**
 * @brief Write then read data from the 1-Wire target device with matching rom.
 *
 * This routine uses w1_reset_select to select the given rom_id.
 * Then writes given data and reads the response back from the device.
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param rom       Pointer to the ROM id structure.
 * @param write_buf Pointer to the data to be written.
 * @param write_len Number of bytes to write.
 * @param read_buf  Pointer to storage for read data.
 * @param read_len  Number of bytes to read.
 *
 * @retval 0 If successful.
 * @retval -EIO  If no device is connected to the bus.

 */
int w1_write_read(const struct device *dev, const struct w1_rom *rom, const uint8_t *write_buf,
                  size_t write_len, uint8_t *read_buf, size_t read_len);

/**
 * @brief Search 1-wire devices on the bus.
 *
 * This function searches devices on the 1-wire bus, with the possibility
 * to search either all devices or only devices that have an active alarm state.
 * If a callback_isr is passed, the callback is called for each found device.
 *
 * The algorithm mostly follows the suggestions of
 * https://pdfserv.maximintegrated.com/en/an/AN187.pdf
 *
 * Note: Filtering on families is not supported.
 *
 * @param dev           Pointer to the device structure for the driver instance.
 * @param command       Can either be W1_SEARCH_ALARM or W1_SEARCH_ROM.
 * @param family        W1_SEARCH_ALL_FAMILIES searcheas all families, filtering
 *                      on a specific family is not yet supported.
 * @param callback_isr  Is called for each found device.
 * @param callback_arg  This will be passed whenever the isr is called.
 *
 * @retval              Number of devices found.
 */
size_t w1_search_bus(const struct device *dev, uint8_t command, uint8_t family,
                     w1_search_callback_t callback_isr, void *callback_arg);

/**
 * @brief Search for 1-Wire device on bus.
 *
 * This routine can discover unknown devices on the bus by scanning for the
 * unique 64-bit registration number.
 *
 * @param dev           Pointer to the device structure for the driver instance.
 * @param callback_isr  Is called for each found device.
 * @param callback_arg  This will be passed whenever the isr is called.
 *
 * @retval              Number of devices found.
 */
static inline size_t w1_search_rom(const struct device *dev, w1_search_callback_t callback_isr,
                                   void *callback_arg)
{
	return w1_search_bus(dev, W1_CMD_SEARCH_ROM, W1_SEARCH_ALL_FAMILIES, callback_isr,
	                     callback_arg);
}

/**
 * @brief Search for 1-Wire devices with an active alarm.
 *
 * This routine searches 1-Wire devices on the bus, which currently have
 * an active alarm.
 *
 * @param dev           Pointer to the device structure for the driver instance.
 * @param callback_isr  Is called for each found device.
 * @param callback_arg  This will be passed whenever the isr is called.
 *
 * @retval              Number of devices found.
 */
static inline size_t w1_search_alarm(const struct device *dev, w1_search_callback_t callback_isr,
                                     void *callback_arg)
{
	return w1_search_bus(dev, W1_CMD_SEARCH_ALARM, W1_SEARCH_ALL_FAMILIES, callback_isr,
	                     callback_arg);
}

/**
 * @brief Function to convert a w1_rom struct to an uint64_t.
 *
 * @param rom  Pointer to the rom struct.
 *
 * @return     the rom converted to an unsigned integer in host endianness.
 */
static inline uint64_t w1_rom_to_uint64(const struct w1_rom *rom)
{
	return sys_get_be64((uint8_t *)rom);
}

/**
 * @brief Function to write an uint64_t to struct w1_rom ptr.
 *
 * @param rom64 Unsigned 64 bit integer representing the rom in host endianness.
 * @param rom   The rom struct ptr.
 */
static inline void w1_uint64_to_rom(const uint64_t rom64, struct w1_rom *rom)
{
	sys_put_be64(rom64, (uint8_t *)rom);
}

/**
 * @brief Compute CRC-8 chacksum as defined in the 1-Wire specification.
 *
 * The 1-Wire of CRC 8 variant is using 0x31 as its polynomial with the initial
 * value set to 0x00.
 * This CRC is used to check the correctness of the unique 56-bit ROM ID.
 *
 * @param src  Input bytes for the computation.
 * @param len  Length of the input in bytes.
 *
 * @return     The computed CRC8 value.
 */
static inline uint8_t w1_crc8(const uint8_t *src, size_t len)
{
	return crc8(src, len, W1_CRC8_POLYNOMIAL, W1_CRC8_INITIAL_VALUE, true);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_W1_H_ */
