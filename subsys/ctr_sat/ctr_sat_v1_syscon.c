/* CHESTER includes */
#include "astronode_s.h"

#include <chester/ctr_sat.h>

/* Zephyr includes */
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_sat_v1_syscon, CONFIG_CTR_SAT_LOG_LEVEL);

static void ctr_sat_uart_callback(const struct device *dev, void *user_data);
static void ctr_sat_event_gpio_callback(const struct device *dev, struct gpio_callback *cb,
					uint32_t pin_mask);

int ctr_sat_v1_syscon_uart_write_read(struct ctr_sat *sat);
int ctr_sat_v1_syscon_gpio_write(struct ctr_sat *sat, enum ctr_sat_pin pin, bool value);

int ctr_sat_init_generic(struct ctr_sat *sat);

int ctr_sat_v1_init_syscon(struct ctr_sat *sat)
{
	int ret;

	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	sat_syscon->uart_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740_syscon));

	if (!device_is_ready(sat_syscon->uart_dev)) {
		LOG_ERR("UART Device not ready");
		return -ENODEV;
	}

	uart_irq_callback_user_data_set(sat_syscon->uart_dev, ctr_sat_uart_callback, sat);

	struct gpio_dt_spec reset_spec =
		GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_reset_gpios);
	struct gpio_dt_spec wakeup_spec =
		GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_wakeup_gpios);
	struct gpio_dt_spec event_spec =
		GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_event_gpios);

	memcpy(&sat_syscon->reset_gpio, &reset_spec, sizeof(struct gpio_dt_spec));
	memcpy(&sat_syscon->wakeup_gpio, &wakeup_spec, sizeof(struct gpio_dt_spec));
	memcpy(&sat_syscon->event_gpio, &event_spec, sizeof(struct gpio_dt_spec));

	k_poll_signal_init(&sat_syscon->rx_completed_signal);

	ret = gpio_pin_configure_dt(&sat_syscon->reset_gpio, GPIO_OUTPUT | GPIO_ACTIVE_HIGH);
	if (ret) {
		LOG_DBG("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_set_dt(&sat_syscon->reset_gpio, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&sat_syscon->event_cb, ctr_sat_event_gpio_callback,
			   BIT(sat_syscon->event_gpio.pin));

	ret = gpio_add_callback(sat_syscon->event_gpio.port, &sat_syscon->event_cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&sat_syscon->event_gpio, GPIO_INT_EDGE_RISING);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

	sat->ctr_sat_uart_write_read = ctr_sat_v1_syscon_uart_write_read;
	sat->ctr_sat_gpio_write = ctr_sat_v1_syscon_gpio_write;

	return ctr_sat_init_generic(sat);
}

int ctr_sat_v1_syscon_uart_write_read(struct ctr_sat *sat)
{
	int ret;

	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	sat_syscon->tx_buf_transmit_ptr = sat->tx_buf;
	sat_syscon->rx_buf_receive_ptr = sat->rx_buf;

	k_poll_signal_reset(&sat_syscon->rx_completed_signal);

	uart_irq_rx_enable(sat_syscon->uart_dev);
	uart_irq_tx_enable(sat_syscon->uart_dev);

	/* wait for reply */
	struct k_poll_event events[1] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &sat_syscon->rx_completed_signal),
	};

	/*
		100 msec: max time to respond by module

		100 msec: pesimistic assumption of delay by I2C -> UART or W1 -> I2C -> UART bridges

		1000 * tx_buf_len * 10 / 9600: delay of command transmission (1000 = conversion sec
	   to msec, * 10 = 10 bauds per byte, 9600 = baudrate)

		1000 * RX_MESSAGE_MAX_SIZE * 10 / 9600: delay of answer transmission (the same as
	   for command)

	*/
	long timeout_ms = 100 + 100 + (1000 * sat->tx_buf_len * 10 / 9600) +
			  (1000 * RX_MESSAGE_MAX_SIZE * 10 / 9600);
	k_timeout_t timeout = K_MSEC(timeout_ms);
	ret = k_poll(events, ARRAY_SIZE(events), timeout);
	if (ret == -EAGAIN) {
		LOG_ERR("Command failed because Astronode module did not respond within specified "
			"time. This may happen when module is disconnected");
		return ret;
	} else if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	if (events->signal->result == -ENOSPC) {
		/* ENOSPC is related to internal buffer, not the user one. For this reason
		 * FLAG_TRIM_LONG_REPLY has no effect here. */
		LOG_ERR("Received message was longer than maximum allowed size %d",
			RX_MESSAGE_MAX_SIZE);
		return -ENOSPC;
	} else if (events->signal->result) {
		LOG_ERR("Error while receiving data %d", events->signal->result);
		return events->signal->result;
	}

	return 0;
}

int ctr_sat_v1_syscon_gpio_write(struct ctr_sat *sat, enum ctr_sat_pin pin, bool value)
{
	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	if (pin == CTR_SAT_PIN_RESET) {
		return gpio_pin_set_dt(&sat_syscon->reset_gpio, value);
	} else if (pin == CTR_SAT_PIN_WAKEUP) {
		return gpio_pin_set_dt(&sat_syscon->wakeup_gpio, value);
	} else {
		return -EINVAL;
	}
}

static void ctr_sat_handle_uart_rx_irq(struct ctr_sat *sat)
{
	int ret;

	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	/*
		TODO remove MIN()
		MIN() is here only because underlaying driver (sc16is7xx) currently returns err when
	   passed buffer is too large
	*/
	int bytes_read = uart_fifo_read(sat_syscon->uart_dev, sat_syscon->rx_buf_receive_ptr,
					MIN(RX_MESSAGE_MAX_SIZE - sat->rx_buf_len, 64));

	if (bytes_read < 0) {
		LOG_ERR("Call `uart_fifo_read` failed: %d", bytes_read);
		return;
	} else if (bytes_read == 0 && RX_MESSAGE_MAX_SIZE - sat->rx_buf_len != 0) {
		LOG_WRN("Call `uart_fifo_read` read 0 bytes");
	}

	bool contains_stop_byte = false;

	for (int i = 0; i < bytes_read; i++) {
		if (sat_syscon->rx_buf_receive_ptr[i] == ASTRONODE_S_END_BYTE) {
			contains_stop_byte = true;
			/* stop byte should be last received byte */
			if (i != bytes_read - 1) {
				LOG_WRN("Stop byte was received but satelite module send "
					"additional data after stop byte!");
			}
			break;
		}
	}

	sat_syscon->rx_buf_receive_ptr += bytes_read;
	sat->rx_buf_len += bytes_read;

	LOG_DBG("Received %d bytes from satelite module. stop_byte=%d, "
		"remaining_buf_size=%d",
		bytes_read, contains_stop_byte, RX_MESSAGE_MAX_SIZE - sat->rx_buf_len);

	if (RX_MESSAGE_MAX_SIZE - sat->rx_buf_len == 0 || contains_stop_byte) {
		int status;

		if (contains_stop_byte) {
			status = 0;
		} else {
			status = -ENOSPC;
		}

		ret = k_poll_signal_raise(&sat_syscon->rx_completed_signal, status);
		if (ret < 0) {
			LOG_ERR("Call `k_poll_signal_raise` failed: %d", ret);
			k_oops();
		}

		LOG_DBG("Disabling RX interrupt");

		uart_irq_rx_disable(sat_syscon->uart_dev);
	}
}

static void ctr_sat_handle_tx_irq(struct ctr_sat *sat)
{
	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	if (sat->tx_buf_len == 0) {
		LOG_DBG("TX completed. No bytes remains to send. Disabling TX interrupt");
		uart_irq_tx_disable(sat_syscon->uart_dev);
		return;
	}

	/*
		TODO remove MIN()
		MIN() is here only because underlaying driver (sc16is7xx) currently returns err when
	   passed buffer is too large
	*/
	int bytes_sent = uart_fifo_fill(sat_syscon->uart_dev, sat_syscon->tx_buf_transmit_ptr,
					MIN(sat->tx_buf_len, 64));
	if (bytes_sent < 0) {
		LOG_ERR("Call `uart_fifo_fill` failed: %d", bytes_sent);
		return;
	}

	sat_syscon->tx_buf_transmit_ptr += bytes_sent;
	sat->tx_buf_len -= bytes_sent;

	if (bytes_sent == 0) {
		LOG_ERR("Call `uart_fifo_fill` accepted 0 bytes!");
	} else {
		LOG_DBG("Written %u byte(s) to FIFO. %u bytes remains", bytes_sent,
			sat->tx_buf_len);
	}
}

static void ctr_sat_uart_callback(const struct device *dev, void *user_data)
{
	struct ctr_sat *sat = (struct ctr_sat *)user_data;
	struct ctr_sat_syscon *sat_syscon = &sat->hw_abstraction.syscon;

	if (!uart_irq_update(sat_syscon->uart_dev)) {
		LOG_WRN("uart_irq_update did not allow execution of UART interrupt handler. "
			"Possible spurious interrupt");
		return;
	}

	int is_handled = 0;

	if (uart_irq_rx_ready(sat_syscon->uart_dev)) {
		is_handled = 1;
		ctr_sat_handle_uart_rx_irq(sat);
	}

	if (uart_irq_tx_ready(sat_syscon->uart_dev)) {
		is_handled = 1;
		ctr_sat_handle_tx_irq(sat);
	}

	if (!is_handled) {
		LOG_WRN("Possible spurious UART interrupt. No TX and no RX flag was set");
	}
}

static void ctr_sat_event_gpio_callback(const struct device *dev, struct gpio_callback *cb,
					uint32_t pin_mask)
{
	struct ctr_sat *sat = CONTAINER_OF(cb, struct ctr_sat, hw_abstraction.syscon.event_cb);

	k_sem_give(&sat->event_trig_sem);
}
