/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <nrfx_twis.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define I2C_SLAVE_ADDR 0x08
#define I2C_SCL_PIN    5
#define I2C_SDA_PIN    4

static const nrfx_twis_t m_twis = NRFX_TWIS_INSTANCE(1);

static void twis_event_handler(nrfx_twis_evt_t const *const p_event)
{
	nrfx_err_t ret_err;

	static uint8_t rx_buf[32];
	static uint8_t tx_buf[32];

	size_t len;

	uint32_t err_flags;

	LOG_DBG("Event: %d", p_event->type);

	switch (p_event->type) {
	case NRFX_TWIS_EVT_READ_REQ:
		if (p_event->data.buf_req) {
			ret_err = nrfx_twis_tx_prepare(&m_twis, tx_buf, sizeof(tx_buf));
			if (ret_err != NRFX_SUCCESS) {
				LOG_ERR("Call `nrfx_twis_tx_prepare` failed: 0x%08x", ret_err);
				k_oops();
			}
		}
		break;
	case NRFX_TWIS_EVT_READ_DONE:
		break;
	case NRFX_TWIS_EVT_READ_ERROR:
		err_flags = nrfx_twis_error_get_and_clear(&m_twis);
		LOG_ERR("Read error: 0x%08x", err_flags);
		break;
	case NRFX_TWIS_EVT_WRITE_REQ:
		if (p_event->data.buf_req) {
			ret_err = nrfx_twis_rx_prepare(&m_twis, rx_buf, sizeof(rx_buf));
			if (ret_err != NRFX_SUCCESS) {
				LOG_ERR("Call `nrfx_twis_rx_prepare` failed: %d", ret_err);
				k_oops();
			}
		}
		break;
	case NRFX_TWIS_EVT_WRITE_DONE:
		len = nrfx_twis_rx_amount(&m_twis);
		LOG_HEXDUMP_DBG(rx_buf, len, "RX buffer");
		memcpy(tx_buf, rx_buf, MIN(len, sizeof(tx_buf)));
		break;
	case NRFX_TWIS_EVT_WRITE_ERROR:
		err_flags = nrfx_twis_error_get_and_clear(&m_twis);
		LOG_ERR("Write error: 0x%08x", err_flags);
		break;
	case NRFX_TWIS_EVT_GENERAL_ERROR:
		err_flags = nrfx_twis_error_get_and_clear(&m_twis);
		LOG_ERR("General error: 0x%08x", err_flags);
		break;
	default:
		LOG_WRN("Unhandled event: %d", p_event->type);
		break;
	}
}

void main(void)
{
	nrfx_err_t ret_nrfx;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TWIS1), 0, nrfx_twis_1_irq_handler, NULL, 0);
	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_TWIS1));

	nrfx_twis_config_t config =
		NRFX_TWIS_DEFAULT_CONFIG(I2C_SCL_PIN, I2C_SDA_PIN, I2C_SLAVE_ADDR);

	ret_nrfx = nrfx_twis_init(&m_twis, &config, twis_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_twis_init` failed: 0x%08x", ret_nrfx);
		k_oops();
	}

	nrfx_twis_enable(&m_twis);

	for (;;) {
		LOG_INF("Alive");
		k_sleep(K_SECONDS(10));
	}
}
