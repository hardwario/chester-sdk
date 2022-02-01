/*
 * Copyright (c) 2021 Thomas Stranger
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief 1-Wire network related functions.
 *
 * The following procedures wrap basic w1 syscalls, they should be callable
 * from user mode as well as supervisor mode, therefore _ZEPHYR_SUPERVISOR__
 * is not defined for this file such that inline macros do not skip
 * the arch_is_user_context() check.
 */

#include <logging/log.h>
#include <drivers/w1.h>

int w1_read_rom(const struct device *dev, struct w1_rom *rom_id)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (!w1_reset_bus(dev)) {
		ret = -EIO;
		goto out;
	}

	w1_write_byte(dev, W1_CMD_READ_ROM);
	w1_read_block(dev, (uint8_t *)rom_id, sizeof(struct w1_rom));

	if (w1_crc8((uint8_t *)rom_id, sizeof(struct w1_rom)) != 0) {
		ret = -EIO;
	}

out:
	w1_unlock_bus(dev);
	return ret;
};

int w1_match_rom(const struct device *dev, const struct w1_rom *rom_id)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (!w1_reset_bus(dev)) {
		ret = -EIO;
		goto out;
	}

	w1_write_byte(dev, W1_CMD_MATCH_ROM);
	w1_write_block(dev, (uint8_t *)rom_id, 8);
out:
	w1_unlock_bus(dev);
	return ret;
};

int w1_resume_command(const struct device *dev)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (!w1_reset_bus(dev)) {
		ret = -EIO;
		goto out;
	}

	w1_write_byte(dev, W1_CMD_RESUME);
out:
	w1_unlock_bus(dev);
	return ret;

}

int w1_skip_rom(const struct device *dev)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (!w1_reset_bus(dev)) {
		ret = -EIO;
		goto out;
	}

	w1_write_byte(dev, W1_CMD_SKIP_ROM);
out:
	w1_unlock_bus(dev);
	return ret;
};

int w1_reset_select(const struct device *dev, const struct w1_rom *rom_id)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (!w1_reset_bus(dev)) {
		ret = -EIO;
		goto out;
	}

	if (w1_get_peripheral_count(dev) > 1) {
		w1_write_byte(dev, W1_CMD_MATCH_ROM);
		w1_write_block(dev, (uint8_t *)rom_id, 8);
	} else {
		w1_write_byte(dev, W1_CMD_SKIP_ROM);
	}

out:
	w1_unlock_bus(dev);
	return ret;
}

int w1_write_read(const struct device *dev, const struct w1_rom *rom,
		   const uint8_t *write_buf, size_t write_len,
		   uint8_t *read_buf, size_t read_len)
{
	int ret = 0;

	w1_lock_bus(dev);
	if (w1_reset_select(dev, rom) != 0) {
		ret = -EIO;
		goto out;
	}

	w1_write_block(dev, write_buf, write_len);
	if (read_buf != NULL) {
		w1_read_block(dev, read_buf, read_len);
	}

out:
	w1_unlock_bus(dev);
	return ret;
};
