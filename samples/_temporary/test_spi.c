#include "test_spi.h"
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/spi.h>
#include <stdio.h>

static int read(const struct device *spi, struct spi_config *spi_cfg, uint8_t *output, size_t count)
{
	uint8_t buffer_tx[] = { 0x9f };

	const struct spi_buf tx_buf[] = { { .buf = buffer_tx, .len = sizeof(buffer_tx) },
		                          { .buf = NULL, .len = 3 } };

	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 2 };

	const struct spi_buf rx_buf[] = { { .buf = NULL, .len = 1 },
		                          { .buf = output, .len = count } };

	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 2 };

	return spi_transceive(spi, spi_cfg, &tx, &rx);
}

static int sleep(const struct device *spi, struct spi_config *spi_cfg)
{
	uint8_t buffer_tx[] = { 0x79 };

	const struct spi_buf tx_buf[] = { { .buf = buffer_tx, .len = sizeof(buffer_tx) } };

	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 1 };

	const struct spi_buf rx_buf[] = { { .buf = NULL, .len = 1 } };

	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 1 };

	return spi_transceive(spi, spi_cfg, &tx, &rx);
}

void test_spi(void)
{
	printf("TEST: SPI\n");

	const struct device *dev = device_get_binding("SPI_1");

	if (dev == NULL) {
		printf("Could not get SPI device\n");
		for (;;)
			;
	}

	struct spi_cs_control spi_cs = { 0 };

	spi_cs.gpio_dev = device_get_binding("GPIO_0");
	spi_cs.gpio_pin = 23;
	spi_cs.gpio_dt_flags = GPIO_ACTIVE_LOW;
	spi_cs.delay = 2;

	struct spi_config spi_cfg = { 0 };

	spi_cfg.frequency = 500000;
	spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE;
	spi_cfg.cs = &spi_cs;

	uint8_t buffer[3];

	int rc = read(dev, &spi_cfg, buffer, sizeof(buffer));

	if (rc < 0) {
		printf("Error reading from SPI (%d)\n", rc);
		for (;;)
			;
	} else {
		printf("B0 %02x\n", buffer[0]);
		printf("B1 %02x\n", buffer[1]);
		printf("B2 %02x\n", buffer[2]);
	}

	k_sleep(K_MSEC(100));

	rc = sleep(dev, &spi_cfg);

	if (rc < 0) {
		printf("Error sleeping NOR (%d)\n", rc);
		for (;;)
			;
	}

	printf("... OK\n");
}
