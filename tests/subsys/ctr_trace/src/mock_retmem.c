/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "mock_retmem.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/retained_mem.h>
#include <zephyr/kernel.h>

#include <string.h>

struct mock_retmem_config {
	uint8_t *base;
	size_t size;
};

static uint8_t mock_chosen_backing[MOCK_RETMEM_SIZE];
uint8_t mock_test_backing[MOCK_RETMEM_SIZE];

static int mock_retmem_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

static ssize_t mock_retmem_size(const struct device *dev)
{
	const struct mock_retmem_config *config = dev->config;

	return (ssize_t)config->size;
}

static int mock_retmem_read(const struct device *dev, off_t offset, uint8_t *buffer, size_t size)
{
	const struct mock_retmem_config *config = dev->config;

	memcpy(buffer, config->base + offset, size);
	return 0;
}

static int mock_retmem_write(const struct device *dev, off_t offset, const uint8_t *buffer,
			     size_t size)
{
	const struct mock_retmem_config *config = dev->config;

	memcpy(config->base + offset, buffer, size);
	return 0;
}

static int mock_retmem_clear(const struct device *dev)
{
	const struct mock_retmem_config *config = dev->config;

	memset(config->base, 0, config->size);
	return 0;
}

static const struct retained_mem_driver_api mock_retmem_api = {
	.size = mock_retmem_size,
	.read = mock_retmem_read,
	.write = mock_retmem_write,
	.clear = mock_retmem_clear,
};

/* Device bound to the DT chosen node, used by the ctr_trace subsystem. */
static const struct mock_retmem_config mock_chosen_config = {
	.base = mock_chosen_backing,
	.size = MOCK_RETMEM_SIZE,
};

DEVICE_DT_DEFINE(DT_CHOSEN(ctr_trace_mem), mock_retmem_init, NULL, NULL, &mock_chosen_config,
		 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &mock_retmem_api);

/* Standalone scratch device for the unit tests. */
static const struct mock_retmem_config mock_test_config = {
	.base = mock_test_backing,
	.size = MOCK_RETMEM_SIZE,
};

DEVICE_DEFINE(mock_test_retmem, "MOCK_TEST_RETMEM", mock_retmem_init, NULL, NULL,
	      &mock_test_config, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &mock_retmem_api);

const struct device *mock_test_dev(void)
{
	return DEVICE_GET(mock_test_retmem);
}

void mock_test_reset(void)
{
	memset(mock_test_backing, 0, sizeof(mock_test_backing));
}
