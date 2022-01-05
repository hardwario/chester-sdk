#include <ctr_info.h>

#include <init.h>
#include <logging/log.h>
#include <nrf52840.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#define CRC_BASE 0xFFFFFFFF
#define CRC_POLY 0x04C11DB7

#define SERIAL_NUMBER_OFFSET 0x0C

#define CHECKSUM_OFFSET 0x7C
#define CHECKSUM_LENGTH 4

#define INIT_PRIORITY 99

LOG_MODULE_REGISTER(ctr_info, CONFIG_CTR_INFO_LOG_LEVEL);

static uint8_t m_uicr_customer[sizeof(NRF_UICR->CUSTOMER)];
static bool m_uicr_customer_valid;

static uint32_t calc_crc32(const uint8_t *buf, size_t len)
{
	uint32_t crc = CRC_BASE;

	for (size_t i = 0; i < len; i++) {
		crc ^= buf[i];

		for (int j = 0; j < 8; j++) {
			// Todo: Check if corresponds to CRC_POLY
			crc = (crc >> 1) ^ (0xedb88320 & ~((crc & 1) - 1));
		}
	}

	return ~crc;
}

static bool is_uicr_customer_valid(void)
{
	uint32_t checksum = sys_get_le32(m_uicr_customer + CHECKSUM_OFFSET);
	uint32_t checksum_calc =
	        calc_crc32(m_uicr_customer, sizeof(m_uicr_customer) - CHECKSUM_LENGTH);

	return checksum_calc == checksum;
}

static void load_uicr_customer(void)
{
	for (int i = 0; i < ARRAY_SIZE(NRF_UICR->CUSTOMER); i++) {
		((uint32_t *)m_uicr_customer)[i] = NRF_UICR->CUSTOMER[i];
	}

	m_uicr_customer_valid = is_uicr_customer_valid();

	if (!m_uicr_customer_valid) {
		LOG_WRN("UICR customer section not valid");
	}
}

int ctr_info_get_serial_number(uint32_t *sn)
{
	if (!m_uicr_customer_valid) {
		return -EIO;
	}

	*sn = sys_get_le32(m_uicr_customer + SERIAL_NUMBER_OFFSET);

	return 0;
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	load_uicr_customer();

	return 0;
}

SYS_INIT(init, PRE_KERNEL_1, INIT_PRIORITY);
