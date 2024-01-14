/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <chester/drivers/ctr_lte_if.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DT_DRV_COMPAT hardwario_ctr_lte_if

#define RX_BLOCK_SIZE    64
#define RX_BLOCK_COUNT   2
#define RX_BLOCK_ALIGN   4
#define RX_LINE_MAX_SIZE 1024
#define RX_RING_BUF_SIZE 512
#define RX_TIMEOUT       100000
#define TX_LINE_MAX_SIZE 1024
#define TX_PREFIX        ""
#define TX_PREFIX_LEN    0
#define TX_SUFFIX        "\r\n"
#define TX_SUFFIX_LEN    2

LOG_MODULE_REGISTER(ctr_lte_if, CONFIG_CTR_LTE_IF_LOG_LEVEL);

struct ctr_lte_if_config {
	const struct device *uart_dev;
	const struct gpio_dt_spec reset_spec;
	const struct gpio_dt_spec wakeup_spec;
};

struct ctr_lte_if_data {
	const struct device *dev;
	struct k_work rx_receive_work;
	struct k_work rx_restart_work;
	atomic_t enabled;
	atomic_t reset;
	atomic_t stop;
	ctr_lte_recv_cb recv_cb;
	struct k_mem_slab rx_slab;
	char __aligned(RX_BLOCK_ALIGN) rx_slab_mem[RX_BLOCK_SIZE * RX_BLOCK_COUNT];
	struct k_mutex mut;
	struct k_poll_signal rx_disabled_sig;
	struct k_poll_signal tx_done_sig;
	struct onoff_client onoff_cli;
	struct onoff_manager *onoff_mgr;
	struct ring_buf rx_ring_buf;
	uint8_t rx_ring_buf_mem[RX_RING_BUF_SIZE];
};

static inline const struct ctr_lte_if_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_lte_if_data *get_data(const struct device *dev)
{
	return dev->data;
}

static void rx_receive_work_handler(struct k_work *work)
{
	static bool synced = true;
	static char buf[RX_LINE_MAX_SIZE];
	static size_t len;

	struct ctr_lte_if_data *data = CONTAINER_OF(work, struct ctr_lte_if_data, rx_receive_work);

	if (atomic_get(&data->reset)) {
		LOG_DBG("Reset request");

		ring_buf_reset(&data->rx_ring_buf);

		synced = false;

		atomic_set(&data->reset, false);
	}

	for (char c; ring_buf_get(&data->rx_ring_buf, (uint8_t *)&c, 1) != 0;) {
		if (!synced) {
			if (c == '\r' || c == '\n') {
				synced = true;
			}

			len = 0;

			continue;
		}

		if (c == '\r' || c == '\n') {
			if (len > 0) {
				LOG_DBG("RX: %s", buf);
				if (data->recv_cb != NULL) {
					data->recv_cb(buf);
				}

			} else if (c == '\n') {
				if (data->recv_cb != NULL) {
					data->recv_cb("");
				}
			}

			len = 0;
		} else {
			if (len >= sizeof(buf) - 1) {
				synced = false;

			} else {
				buf[len++] = c;
				buf[len] = '\0';
			}
		}
	}
}

static void rx_restart_work_handler(struct k_work *work)
{
	int ret;
	uint8_t *buf;

	struct ctr_lte_if_data *data = CONTAINER_OF(work, struct ctr_lte_if_data, rx_restart_work);

	ret = k_mem_slab_alloc(&data->rx_slab, (void **)&buf, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		return;
	}

	ret = uart_rx_enable(get_config(data->dev)->uart_dev, buf, RX_BLOCK_SIZE, RX_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&data->rx_slab, buf);
		return;
	}

	atomic_set(&data->reset, true);

	ret = k_work_submit(&data->rx_receive_work);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	int ret;
	uint8_t *buf;

	const struct device *ctr_lte_if_dev = (const struct device *)user_data;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("Event `UART_TX_DONE`");
		k_poll_signal_raise(&get_data(ctr_lte_if_dev)->tx_done_sig, 0);
		break;

	case UART_TX_ABORTED:
		LOG_WRN("Event `UART_TX_ABORTED`");
		k_poll_signal_raise(&get_data(ctr_lte_if_dev)->tx_done_sig, 0);
		break;

	case UART_RX_RDY:
		LOG_DBG("Event `UART_RX_RDY`");

		if (evt->data.rx.len > 0) {
			struct uart_event_rx *rx = &evt->data.rx;

			if (ring_buf_put(&get_data(ctr_lte_if_dev)->rx_ring_buf,
					 rx->buf + rx->offset, rx->len) != rx->len) {
				atomic_set(&get_data(ctr_lte_if_dev)->reset, true);
			}

			ret = k_work_submit(&get_data(ctr_lte_if_dev)->rx_receive_work);
			if (ret < 0) {
				LOG_ERR("Call `k_work_submit` failed: %d", ret);
			}
		}

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("Event `UART_RX_BUF_REQUEST`");

		ret = k_mem_slab_alloc(&get_data(ctr_lte_if_dev)->rx_slab, (void **)&buf,
				       K_NO_WAIT);
		if (ret < 0) {
			LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
			break;
		}

		ret = uart_rx_buf_rsp(dev, buf, RX_BLOCK_SIZE);
		if (ret < 0) {
			LOG_ERR("Call `uart_rx_buf_rsp` failed: %d", ret);
			k_mem_slab_free(&get_data(ctr_lte_if_dev)->rx_slab, buf);
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("Event `UART_RX_BUF_RELEASED`");
		k_mem_slab_free(&get_data(ctr_lte_if_dev)->rx_slab, evt->data.rx_buf.buf);
		break;

	case UART_RX_DISABLED:
		LOG_DBG("Event `UART_RX_DISABLED`");

		if (atomic_get(&get_data(ctr_lte_if_dev)->stop)) {
			k_poll_signal_raise(&get_data(ctr_lte_if_dev)->rx_disabled_sig, 0);

		} else {
			ret = k_work_submit(&get_data(ctr_lte_if_dev)->rx_restart_work);
			if (ret < 0) {
				LOG_ERR("Call `k_work_submit` failed: %d", ret);
			}
		}

		break;

	case UART_RX_STOPPED:
		LOG_WRN("Event `UART_RX_STOPPED`");
		break;
	}
}

static int ctr_lte_if_init_(const struct device *dev, ctr_lte_recv_cb recv_cb)
{
	int ret;

	get_data(dev)->recv_cb = recv_cb;

	ring_buf_init(&get_data(dev)->rx_ring_buf, RX_RING_BUF_SIZE,
		      get_data(dev)->rx_ring_buf_mem);

	k_poll_signal_init(&get_data(dev)->tx_done_sig);
	k_poll_signal_init(&get_data(dev)->rx_disabled_sig);

	if (!device_is_ready(get_config(dev)->uart_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	get_data(dev)->onoff_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (get_data(dev)->onoff_mgr == NULL) {
		LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
		return -ENXIO;
	}

	ret = uart_callback_set(get_config(dev)->uart_dev, uart_callback, (void *)dev);
	if (ret < 0) {
		LOG_ERR("Call `uart_callback_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_lte_if_reset_(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->reset_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(100));

	ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(3000));

	return 0;
}

static int ctr_lte_if_wakeup_(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->wakeup_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->wakeup_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(100));

	ret = gpio_pin_set_dt(&get_config(dev)->wakeup_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int request_hfclk(const struct device *dev)
{
	int ret;

	static struct k_poll_signal sig;

	k_poll_signal_init(&sig);
	sys_notify_init_signal(&get_data(dev)->onoff_cli.notify, &sig);

	ret = onoff_request(get_data(dev)->onoff_mgr, &get_data(dev)->onoff_cli);
	if (ret < 0) {
		LOG_ERR("Call `onoff_request` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_lte_if_enable_(const struct device *dev)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (atomic_get(&get_data(dev)->enabled)) {
		k_mutex_unlock(&get_data(dev)->mut);
		return 0;
	}

	ret = request_hfclk(dev);
	if (ret < 0) {
		LOG_ERR("Call `request_hfclk` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	k_poll_signal_reset(&get_data(dev)->rx_disabled_sig);

	uint8_t *buf;

	ret = k_mem_slab_alloc(&get_data(dev)->rx_slab, (void **)&buf, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	atomic_set(&get_data(dev)->stop, false);

	ret = uart_rx_enable(get_config(dev)->uart_dev, buf, RX_BLOCK_SIZE, RX_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&get_data(dev)->rx_slab, buf);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	atomic_set(&get_data(dev)->enabled, true);

	k_mutex_unlock(&get_data(dev)->mut);

	return 0;
}

static int release_hfclk(const struct device *dev)
{
	int ret;

	ret = onoff_cancel_or_release(get_data(dev)->onoff_mgr, &get_data(dev)->onoff_cli);
	if (ret < 0) {
		LOG_ERR("Call `onoff_cancel_or_release` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_lte_if_disable_(const struct device *dev)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (!atomic_get(&get_data(dev)->enabled)) {
		k_mutex_unlock(&get_data(dev)->mut);
		return 0;
	}

	atomic_set(&get_data(dev)->stop, true);

	ret = uart_rx_disable(get_config(dev)->uart_dev);
	if (ret < 0) {
		LOG_ERR("Call `uart_rx_disable` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &get_data(dev)->rx_disabled_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	ret = release_hfclk(dev);
	if (ret < 0) {
		LOG_ERR("Call `release_hfclk` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	atomic_set(&get_data(dev)->enabled, false);

	k_mutex_unlock(&get_data(dev)->mut);

	return 0;
}

static int ctr_lte_if_send_(const struct device *dev, const char *fmt, va_list ap)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (!atomic_get(&get_data(dev)->enabled)) {
		k_mutex_unlock(&get_data(dev)->mut);
		return -EBUSY;
	}

	static char buf[TX_LINE_MAX_SIZE];

	ret = vsnprintf(&buf[TX_PREFIX_LEN], sizeof(buf) - TX_PREFIX_LEN - TX_SUFFIX_LEN, fmt, ap);
	if (ret < 0) {
		LOG_ERR("Call `vsnprintf` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	} else if (ret >= sizeof(buf) - TX_PREFIX_LEN - TX_SUFFIX_LEN) {
		LOG_ERR("Buffer too small");
		k_mutex_unlock(&get_data(dev)->mut);
		return -ENOBUFS;
	}

	LOG_INF("TX: %s", &buf[TX_PREFIX_LEN]);

	size_t len = strlen(&buf[TX_PREFIX_LEN]);

	memcpy(&buf[0], TX_PREFIX, TX_PREFIX_LEN);
	len += TX_PREFIX_LEN;

	memcpy(&buf[len], TX_SUFFIX, TX_SUFFIX_LEN);
	len += TX_SUFFIX_LEN;

	k_poll_signal_reset(&get_data(dev)->tx_done_sig);

	ret = uart_tx(get_config(dev)->uart_dev, buf, len, SYS_FOREVER_MS);
	if (ret < 0) {
		LOG_ERR("Call `uart_tx` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &get_data(dev)->tx_done_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->mut);

	return 0;
}

static int ctr_lte_if_send_raw_(const struct device *dev, const void *buf, size_t len)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (!atomic_get(&get_data(dev)->enabled)) {
		k_mutex_unlock(&get_data(dev)->mut);
		return -EBUSY;
	}

	LOG_INF("TX (raw): %u byte(s)", len);

	k_poll_signal_reset(&get_data(dev)->tx_done_sig);

	ret = uart_tx(get_config(dev)->uart_dev, buf, len, SYS_FOREVER_MS);
	if (ret < 0) {
		LOG_ERR("Call `uart_tx` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &get_data(dev)->tx_done_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->mut);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->mut);

	return 0;
}

static int ctr_lte_if_drv_init(const struct device *dev)
{
	int ret;

	k_work_init(&get_data(dev)->rx_receive_work, rx_receive_work_handler);
	k_work_init(&get_data(dev)->rx_restart_work, rx_restart_work_handler);
	k_mutex_init(&get_data(dev)->mut);
	k_mem_slab_init(&get_data(dev)->rx_slab, get_data(dev)->rx_slab_mem, RX_BLOCK_SIZE,
			RX_BLOCK_COUNT);

	if (!device_is_ready(get_config(dev)->reset_spec.port)) {
		LOG_ERR("Device not ready: Reset");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->reset_spec, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Unable to configure reset pin: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->wakeup_spec.port)) {
		LOG_ERR("Device not ready: Wakeup");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->wakeup_spec, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Unable to configure wakeup pin: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_lte_if_driver_api ctr_lte_if_driver_api = {
	.init = ctr_lte_if_init_,
	.reset = ctr_lte_if_reset_,
	.wakeup = ctr_lte_if_wakeup_,
	.enable = ctr_lte_if_enable_,
	.disable = ctr_lte_if_disable_,
	.send = ctr_lte_if_send_,
	.send_raw = ctr_lte_if_send_raw_,
};

#define CTR_LTE_IF_INIT(n)                                                                         \
	static const struct ctr_lte_if_config inst_##n##_config = {                                \
		.uart_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                         \
		.reset_spec = GPIO_DT_SPEC_INST_GET(n, reset_gpios),                               \
		.wakeup_spec = GPIO_DT_SPEC_INST_GET(n, wakeup_gpios),                             \
	};                                                                                         \
	static struct ctr_lte_if_data inst_##n##_data = {                                          \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_lte_if_drv_init, NULL, &inst_##n##_data, &inst_##n##_config,  \
			      POST_KERNEL, CONFIG_SERIAL_INIT_PRIORITY, &ctr_lte_if_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_LTE_IF_INIT)
