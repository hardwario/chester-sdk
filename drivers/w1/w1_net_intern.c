/*
 * Copyright (c) 2021 Thomas Stranger
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Private 1-Wire network related functions.
 */

#include <logging/log.h>
#include <drivers/w1.h>

LOG_MODULE_REGISTER(w1, CONFIG_W1_LOG_LEVEL);

#define W1_SEARCH_DISCREPANCY_INIT 0
#define W1_SEARCH_LAST_DEVICE 65
#define W1_SEARCH_NO_DEVICE 66

/* @brief searches bus for next device
 *
 * This function searches the next 1-wire device on the bus.
 * It sets the found rom and the last discrepancy in case more than one
 * device took part in the search.
 * In case only one device took part in the search, the discrepancy is set to
 * W1_SEARCH_LAST_DEVICE, and in case no device participated in the search,
 * the discrepancy is set to W1_SEARCH_NO_DEVICE.
 *
 * The Implementation is similar as suggested in the maxim application note 187.
 * The controller reads the first rom bit and its complementary value of all
 * devices.
 * Due to physical characteristics, the value received is a
 * logical AND of all devices 1st bit. Devices only continue to participate
 * in the search procedure if the next bit the controller sends matches their
 * own addresses bit. This allows the controller to branch through 64-bit
 * addresses in order to detect all devices.

 * The 1st bit received is stored in bit 1 of rom_inv_64, the 2nd in bit 2 and so
 * on until bit 64.
 * As a result each byte of the rom has the correct bit order, but the received
 * bytes (big-endian) stored in rom_inv_64 are in inverse byte order.
 *
 * Note: Filtering on families is not supported.
 *
 * @param dev              Pointer to the device structure for the w1 instance.
 * @param command          Command to chose between normal and alarm search.
 * @param family           The family is ignored in search.
 * @param last_discrepancy This must be set to W1_SEARCH_DISCREPANCY_INIT before
 *                         the first call, it carries the search progress for
 *                         further calls.
 * @param rom_inv_64       The found rom: It must be set to zero before first
 *                         call and carries the last found rom for furter calls.
 *                         The rom is stored in inverse byte order.
 */
static void search_device(const struct device *dev, uint8_t command, uint8_t family,
                          size_t *last_discrepancy, uint64_t *rom_inv_64)
{
	size_t next_discrepancy;

	ARG_UNUSED(family);
	__ASSERT_NO_MSG(command == W1_CMD_SEARCH_ROM || command == W1_CMD_SEARCH_ALARM);

	if (w1_reset_bus(dev) == 0) {
		*last_discrepancy = W1_SEARCH_NO_DEVICE;
		return;
	}

	w1_write_byte(dev, command);
	next_discrepancy = W1_SEARCH_LAST_DEVICE;

	for (size_t id_bit_nr = 1; id_bit_nr < 65; id_bit_nr++) {
		bool last_id_bit;
		bool last_complement_id_bit;

		last_id_bit = w1_read_bit(dev);
		last_complement_id_bit = w1_read_bit(dev);

		if (last_id_bit && last_complement_id_bit) {
			/*
			 * No device participating:
			 * We can stop following the branch.
			 */
			LOG_DBG("No device paricipating");
			*last_discrepancy = W1_SEARCH_NO_DEVICE;
			return;
		} else if (last_id_bit != last_complement_id_bit) {
			/*
			 * All devices connected have same ROM bit value:
			 * We can directly follow last_id_bit branch.
			 */
		} else {
			/*
			 * (!last_id_bit && !last_complement_id_bit)
			 * Discrepancy detected: bit value at id_bit_nr does
			 * not match for all devices on the bus.
			 */
			if ((id_bit_nr > *last_discrepancy) ||
			    ((id_bit_nr < *last_discrepancy) &&
			     (*rom_inv_64 & BIT64(id_bit_nr - 1)))) {
				/*
				 * - id_bit_nr > last_discrepancy:
				 *     Start always w/ branch of 1s
				 * - id_bit_nr < last_discrepancy:
				 *     Follow same branch as before
				 */
				last_id_bit = true;
				next_discrepancy = id_bit_nr;
			} else {
				/*
				 * - id_bit_nr == last_discrepancy:
				 *     1-path already done, therefore go 0 path
				 * - id_bit_nr < last_discrepancy:
				 *     Follow same branch as before
				 */
			}
		}

		/*
		 * Send and store the chosen bit: all not matching devices will
		 * no longer participate in this search until they are reset.
		 */
		w1_write_bit(dev, last_id_bit);
		*rom_inv_64 &= ~BIT64(id_bit_nr - 1);
		*rom_inv_64 |= last_id_bit ? BIT64(id_bit_nr - 1) : 0;
	}

	*last_discrepancy = next_discrepancy;
}

size_t w1_search_bus(const struct device *dev, uint8_t command, uint8_t family,
                     w1_search_callback_t callback_isr, void *callback_arg)
{
	size_t last_discrepancy = W1_SEARCH_DISCREPANCY_INIT;
	uint64_t found_rom_inv_64 = 0;
	struct w1_rom found_rom = { 0 };
	size_t found_cnt = 0;

	w1_lock_bus(dev);

	do {
		search_device(dev, command, family, &last_discrepancy, &found_rom_inv_64);
		if (last_discrepancy == W1_SEARCH_NO_DEVICE) {
			break;
		}

		found_cnt++;
		/*
		 * rom is stored in found_rom_inv_64 in "inverse byte order" =>
		 * Only big-endian targets need to swap, such that struct's
		 * bytes are stored in big-endian byte order.
		 */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		sys_memcpy_swap(&found_rom, &found_rom_inv_64, 8);
#else
		*(uint64_t *)&found_rom = found_rom_inv_64;
#endif
		LOG_DBG("rom-found: nr.%u, %016llx", found_cnt, w1_rom_to_uint64(&found_rom));

		if (callback_isr != NULL) {
			callback_isr(found_rom, callback_arg);
		}

	} while (last_discrepancy != W1_SEARCH_LAST_DEVICE);

	w1_unlock_bus(dev);

	return found_cnt;
}
