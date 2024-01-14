/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_lte_if_v2.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DT_DRV_COMPAT hardwario_ctr_lte_if_v2

LOG_MODULE_REGISTER(ctr_lte_if_v2, CONFIG_CTR_LTE_IF_V2_LOG_LEVEL);

#define RESET_DELAY   K_MSEC(100)
#define RESET_PAUSE   K_SECONDS(3)
#define WAKE_UP_DELAY K_MSEC(100)
#define WAKE_UP_PAUSE K_MSEC(100)

#define TX_LINE_PREFIX ""
#define TX_LINE_SUFFIX "\r\n"

#define TX_LINE_BUF_SIZE 1024
#define RX_LINE_BUF_SIZE 1024

#define RX_HEAP_MEM_SIZE    4096
#define RX_PIPE_BUF_SIZE    512
#define RX_SLAB_BLOCK_ALIGN 4
#define RX_SLAB_BLOCK_COUNT 2
#define RX_SLAB_BLOCK_SIZE  64
#define RX_TIMEOUT_US       100000

struct ctr_lte_if_v2_config {
	const struct device *uart_dev;
	const struct gpio_dt_spec reset_spec;
	const struct gpio_dt_spec wakeup_spec;
};

struct ctr_lte_if_v2_data {
	atomic_t in_dialog;
	atomic_t in_data_mode;
	atomic_t rx_line_synced;
	atomic_t stop_request;
	bool enabled;
	char rx_line_buf[RX_LINE_BUF_SIZE];
	char rx_slab_mem[RX_SLAB_BLOCK_SIZE * RX_SLAB_BLOCK_COUNT] __aligned(RX_SLAB_BLOCK_ALIGN);
	char tx_line_buf[TX_LINE_BUF_SIZE];
	const struct device *dev;
	ctr_lte_if_v2_user_cb user_cb;
	size_t rx_line_len;
	struct k_fifo rx_fifo;
	struct k_heap rx_heap;
	struct k_mem_slab rx_slab;
	struct k_mutex lock;
	struct k_pipe rx_pipe;
	struct k_sem rx_disabled_sem;
	struct k_sem tx_finished_sem;
	struct k_work rx_loss_work;
	struct k_work rx_receive_work;
	struct k_work rx_restart_work;
	struct onoff_client onoff_cli;
	struct onoff_manager *onoff_mgr;
	uint8_t rx_heap_mem[RX_HEAP_MEM_SIZE];
	uint8_t rx_pipe_buf[RX_PIPE_BUF_SIZE];
	void *user_data;
};

static inline const struct ctr_lte_if_v2_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_lte_if_v2_data *get_data(const struct device *dev)
{
	return dev->data;
}

static void process_line(const struct device *dev)
{
	int ret;

	struct ctr_lte_if_v2_data *data = get_data(dev);

	size_t len = strlen(data->rx_line_buf);
	if (len) {
		LOG_DBG("Stacking line: %s", data->rx_line_buf);
	} else {
		LOG_DBG("Stacking empty line");
	}

	char *p = k_heap_alloc(&data->rx_heap, len + 1, K_NO_WAIT);
	if (!p) {
		LOG_ERR("Call `k_heap_alloc` failed");
		if (data->user_cb) {
			data->user_cb(data->dev, CTR_LTE_IF_V2_EVENT_RX_LOSS, data->user_data);
		}
		return;
	} else {
		LOG_DBG("Allocated from heap: %p - %u byte(s)", (void *)p, len + 1);
	}

	strcpy(p, data->rx_line_buf);

	ret = k_fifo_alloc_put(&data->rx_fifo, p);
	if (ret) {
		LOG_ERR("Call `k_fifo_alloc_put` failed: %d", ret);
		k_heap_free(&data->rx_heap, p);
		LOG_DBG("Released from heap: %p", (void *)p);
	}

	if (data->user_cb && !atomic_get(&data->in_dialog)) {
		data->user_cb(data->dev, CTR_LTE_IF_V2_EVENT_RX_LINE, data->user_data);
	}
}

static void receive_in_line_mode(const struct device *dev)
{
	int ret;

	struct ctr_lte_if_v2_data *data = get_data(dev);

	for (;;) {
		char c;

		size_t bytes_read;
		ret = k_pipe_get(&data->rx_pipe, &c, 1, &bytes_read, 1, K_NO_WAIT);
		if (ret) {
			break;
		}

		if (c == '\r') {
			continue;
		} else if (!atomic_get(&data->rx_line_synced)) {
			data->rx_line_len = 0;
			if (c == '\n') {
				atomic_set(&data->rx_line_synced, true);
			}
		} else if (c == '\n') {
			data->rx_line_buf[data->rx_line_len] = '\0';
			data->rx_line_len = 0;
			process_line(dev);
			atomic_set(&data->rx_line_synced, false);
		} else if (data->rx_line_len >= sizeof(data->rx_line_buf) - 1) {
			atomic_set(&data->rx_line_synced, false);
		} else {
			data->rx_line_buf[data->rx_line_len++] = c;
		}
	}
}

static void receive_in_data_mode(const struct device *dev)
{
	struct ctr_lte_if_v2_data *data = get_data(dev);

	if (data->user_cb && !atomic_get(&data->in_dialog)) {
		data->user_cb(data->dev, CTR_LTE_IF_V2_EVENT_RX_DATA, data->user_data);
	}
}

static void rx_receive_work_handler(struct k_work *work)
{
	struct ctr_lte_if_v2_data *data =
		CONTAINER_OF(work, struct ctr_lte_if_v2_data, rx_receive_work);

	if (atomic_get(&data->in_data_mode)) {
		receive_in_data_mode(data->dev);
	} else {
		receive_in_line_mode(data->dev);
	}
}

static void rx_restart_work_handler(struct k_work *work)
{
	int ret;
	uint8_t *buf;

	struct ctr_lte_if_v2_data *data =
		CONTAINER_OF(work, struct ctr_lte_if_v2_data, rx_restart_work);

	ret = k_mem_slab_alloc(&data->rx_slab, (void **)&buf, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		return;
	}

	k_pipe_flush(&data->rx_pipe);

	atomic_set(&data->rx_line_synced, false);

	ret = uart_rx_enable(get_config(data->dev)->uart_dev, buf, RX_SLAB_BLOCK_SIZE,
			     RX_TIMEOUT_US);
	if (ret) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&data->rx_slab, buf);
		return;
	}

	if (data->user_cb) {
		data->user_cb(data->dev, CTR_LTE_IF_V2_EVENT_RX_LOSS, data->user_data);
	}
}

static void rx_loss_work_handler(struct k_work *work)
{
	struct ctr_lte_if_v2_data *data =
		CONTAINER_OF(work, struct ctr_lte_if_v2_data, rx_loss_work);

	k_pipe_flush(&data->rx_pipe);

	atomic_set(&data->rx_line_synced, false);

	if (data->user_cb) {
		data->user_cb(data->dev, CTR_LTE_IF_V2_EVENT_RX_LOSS, data->user_data);
	}
}

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	int ret;
	uint8_t *buf;

	struct ctr_lte_if_v2_data *data = get_data((const struct device *)user_data);

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("Event `UART_TX_DONE`");
		k_sem_give(&data->tx_finished_sem);
		break;

	case UART_TX_ABORTED:
		LOG_WRN("Event `UART_TX_ABORTED`");
		k_sem_give(&data->tx_finished_sem);
		break;

	case UART_RX_RDY:
		LOG_DBG("Event `UART_RX_RDY`");

		if (evt->data.rx.len) {
			struct uart_event_rx *rx = &evt->data.rx;

#if 1
			LOG_HEXDUMP_DBG(rx->buf + rx->offset, rx->len, "RX buffer");
#endif

			size_t bytes_written;
			ret = k_pipe_put(&data->rx_pipe, rx->buf + rx->offset, rx->len,
					 &bytes_written, rx->len, K_NO_WAIT);

			if (ret) {
				LOG_ERR("Call `k_pipe_put` failed: %d", ret);
				ret = k_work_submit(&data->rx_loss_work);
				if (ret < 0) {
					LOG_ERR("Call `k_work_submit` failed: %d", ret);
				}
			} else {
				ret = k_work_submit(&data->rx_receive_work);
				if (ret < 0) {
					LOG_ERR("Call `k_work_submit` failed: %d", ret);
				}
			}
		}

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("Event `UART_RX_BUF_REQUEST`");

		ret = k_mem_slab_alloc(&data->rx_slab, (void **)&buf, K_NO_WAIT);
		if (ret) {
			LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
			break;
		}

		ret = uart_rx_buf_rsp(dev, buf, RX_SLAB_BLOCK_SIZE);
		if (ret) {
			LOG_ERR("Call `uart_rx_buf_rsp` failed: %d", ret);
			k_mem_slab_free(&data->rx_slab, buf);
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("Event `UART_RX_BUF_RELEASED`");
		k_mem_slab_free(&data->rx_slab, evt->data.rx_buf.buf);
		break;

	case UART_RX_DISABLED:
		LOG_DBG("Event `UART_RX_DISABLED`");

		if (atomic_get(&data->stop_request)) {
			k_sem_give(&data->rx_disabled_sem);
		} else {
			ret = k_work_submit(&data->rx_restart_work);
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

static int ctr_lte_if_v2_set_callback_(const struct device *dev, ctr_lte_if_v2_user_cb user_cb,
				       void *user_data)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void ctr_lte_if_v2_lock_(const struct device *dev)
{
	LOG_INF("Lock");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);
}

static void ctr_lte_if_v2_unlock_(const struct device *dev)
{
	LOG_INF("Unlock");

	k_mutex_unlock(&get_data(dev)->lock);
}

static int ctr_lte_if_v2_reset_(const struct device *dev)
{
	int ret;

	LOG_INF("Reset");

	if (!device_is_ready(get_config(dev)->reset_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(RESET_DELAY);

	ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(RESET_PAUSE);

	return 0;
}

static int ctr_lte_if_v2_wake_up_(const struct device *dev)
{
	int ret;

	LOG_INF("Wake up");

	atomic_set(&get_data(dev)->rx_line_synced, false);

	/* Message "Ready" from modem does not start with <LF>,
	   so we fake it in order to synchronize the reception properly. */

	k_pipe_flush(&get_data(dev)->rx_pipe);

	char c = '\n';
	size_t bytes_written;
	ret = k_pipe_put(&get_data(dev)->rx_pipe, &c, 1, &bytes_written, 1, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_pipe_put` failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->wakeup_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->wakeup_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(WAKE_UP_DELAY);

	ret = gpio_pin_set_dt(&get_config(dev)->wakeup_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	k_sleep(WAKE_UP_PAUSE);

	return 0;
}

static int request_hfclk(const struct device *dev)
{
	int ret;

	struct k_poll_signal sig = {0};
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
	if (ret) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void purge_rx_fifo(const struct device *dev)
{
	char *p;

	while ((p = k_fifo_get(&get_data(dev)->rx_fifo, K_NO_WAIT))) {
		k_heap_free(&get_data(dev)->rx_heap, p);
		LOG_DBG("Released from heap: %p", (void *)p);
	}
}

static int ctr_lte_if_v2_enable_uart_(const struct device *dev)
{
	int ret;

	LOG_INF("Enable UART");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	ret = request_hfclk(dev);
	if (ret) {
		LOG_ERR("Call `request_hfclk` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	uint8_t *buf;
	ret = k_mem_slab_alloc(&get_data(dev)->rx_slab, (void **)&buf, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_sem_reset(&get_data(dev)->rx_disabled_sem);
	k_sem_reset(&get_data(dev)->tx_finished_sem);

	atomic_set(&get_data(dev)->in_dialog, false);
	atomic_set(&get_data(dev)->in_data_mode, false);
	atomic_set(&get_data(dev)->stop_request, false);
	atomic_set(&get_data(dev)->rx_line_synced, false);

	k_pipe_flush(&get_data(dev)->rx_pipe);

	purge_rx_fifo(dev);

	ret = uart_rx_enable(get_config(dev)->uart_dev, buf, RX_SLAB_BLOCK_SIZE, RX_TIMEOUT_US);
	if (ret) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&get_data(dev)->rx_slab, buf);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	get_data(dev)->enabled = true;

	k_mutex_unlock(&get_data(dev)->lock);

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

static int ctr_lte_if_v2_disable_uart_(const struct device *dev)
{
	int ret;

	LOG_INF("Disable UART");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	atomic_set(&get_data(dev)->stop_request, true);

	ret = uart_rx_disable(get_config(dev)->uart_dev);
	if (ret) {
		LOG_ERR("Call `uart_rx_disable` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = k_sem_take(&get_data(dev)->rx_disabled_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = release_hfclk(dev);
	if (ret) {
		LOG_ERR("Call `release_hfclk` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	get_data(dev)->enabled = false;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_enter_data_mode_(const struct device *dev)
{
	LOG_INF("Enter data mode");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_data_mode, true);

	k_pipe_flush(&get_data(dev)->rx_pipe);

	purge_rx_fifo(dev);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_exit_data_mode_(const struct device *dev)
{
	LOG_INF("Exit data mode");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_data_mode, false);

	k_pipe_flush(&get_data(dev)->rx_pipe);

	purge_rx_fifo(dev);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_enter_dialog_(const struct device *dev)
{
	LOG_INF("Enter dialog");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_dialog, true);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_exit_dialog_(const struct device *dev)
{
	LOG_INF("Exit dialog");

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_dialog, false);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_send_line_(const struct device *dev, k_timeout_t timeout,
				    const char *format, va_list ap)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	if (get_data(dev)->in_data_mode) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EPERM;
	}

	strcpy(get_data(dev)->tx_line_buf, TX_LINE_PREFIX);

	size_t size = sizeof(get_data(dev)->tx_line_buf) - strlen(TX_LINE_PREFIX) -
		      strlen(TX_LINE_SUFFIX);

	ret = vsnprintf(&get_data(dev)->tx_line_buf[strlen(TX_LINE_PREFIX)], size, format, ap);
	if (ret < 0) {
		LOG_ERR("Call `vsnprintf` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	} else if (ret >= size) {
		LOG_ERR("Buffer too small");
		k_mutex_unlock(&get_data(dev)->lock);
		return -ENOBUFS;
	}

	LOG_INF("Send line: %s", &get_data(dev)->tx_line_buf[strlen(TX_LINE_PREFIX)]);

	strcat(get_data(dev)->tx_line_buf, TX_LINE_SUFFIX);

	ret = uart_tx(get_config(dev)->uart_dev, get_data(dev)->tx_line_buf,
		      strlen(get_data(dev)->tx_line_buf), SYS_FOREVER_US);
	if (ret) {
		LOG_ERR("Call `uart_tx` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = k_sem_take(&get_data(dev)->tx_finished_sem, timeout);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_recv_line_(const struct device *dev, k_timeout_t timeout, char **line)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	if (get_data(dev)->in_data_mode) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EPERM;
	}

	*line = k_fifo_get(&get_data(dev)->rx_fifo, timeout);
	if (*line) {
		LOG_INF("Receive line: %s", *line);
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_free_line_(const struct device *dev, char *line)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	k_heap_free(&get_data(dev)->rx_heap, line);
	LOG_DBG("Released from heap: %p", (void *)line);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_send_data_(const struct device *dev, k_timeout_t timeout, const void *buf,
				    size_t len)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	if (!get_data(dev)->in_data_mode) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EPERM;
	}

	LOG_INF("Send data: %u byte(s)", len);

	ret = uart_tx(get_config(dev)->uart_dev, buf, len, SYS_FOREVER_US);
	if (ret) {
		LOG_ERR("Call `uart_tx` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = k_sem_take(&get_data(dev)->tx_finished_sem, timeout);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_recv_data_(const struct device *dev, k_timeout_t timeout, void *buf,
				    size_t size, size_t *len)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	if (!get_data(dev)->in_data_mode) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EPERM;
	}

	ret = k_pipe_get(&get_data(dev)->rx_pipe, buf, size, len, 1, timeout);
	if (ret) {
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	LOG_INF("Receive data: %u byte(s)", *len);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_if_v2_drv_init(const struct device *dev)
{
	int ret;

	const struct ctr_lte_if_v2_config *config = get_config(dev);
	struct ctr_lte_if_v2_data *data = get_data(dev);

	k_fifo_init(&data->rx_fifo);
	k_heap_init(&data->rx_heap, data->rx_heap_mem, RX_HEAP_MEM_SIZE);
	k_mem_slab_init(&data->rx_slab, data->rx_slab_mem, RX_SLAB_BLOCK_SIZE, RX_SLAB_BLOCK_COUNT);
	k_mutex_init(&data->lock);
	k_pipe_init(&data->rx_pipe, data->rx_pipe_buf, RX_PIPE_BUF_SIZE);
	k_sem_init(&data->rx_disabled_sem, 0, 1);
	k_sem_init(&data->tx_finished_sem, 0, 1);
	k_work_init(&data->rx_loss_work, rx_loss_work_handler);
	k_work_init(&data->rx_receive_work, rx_receive_work_handler);
	k_work_init(&data->rx_restart_work, rx_restart_work_handler);

	if (!device_is_ready(config->reset_spec.port)) {
		LOG_ERR("Port not ready (RESET)");
		return -ENODEV;
	}

	if (!device_is_ready(config->wakeup_spec.port)) {
		LOG_ERR("Port not ready (WAKEUP)");
		return -ENODEV;
	}

	if (!device_is_ready(config->uart_dev)) {
		LOG_ERR("Bus not ready (UART)");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->reset_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Unable to configure reset pin: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&config->wakeup_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Unable to configure wakeup pin: %d", ret);
		return ret;
	}

	ret = uart_callback_set(config->uart_dev, uart_callback, (void *)dev);
	if (ret) {
		LOG_ERR("Call `uart_callback_set` failed: %d", ret);
		return ret;
	}

	data->onoff_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!data->onoff_mgr) {
		LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
		return -ENXIO;
	}

	return 0;
}

static const struct ctr_lte_if_v2_driver_api ctr_lte_if_v2_driver_api = {
	.set_callback = ctr_lte_if_v2_set_callback_,
	.lock = ctr_lte_if_v2_lock_,
	.unlock = ctr_lte_if_v2_unlock_,
	.reset = ctr_lte_if_v2_reset_,
	.wake_up = ctr_lte_if_v2_wake_up_,
	.enable_uart = ctr_lte_if_v2_enable_uart_,
	.disable_uart = ctr_lte_if_v2_disable_uart_,
	.enter_dialog = ctr_lte_if_v2_enter_dialog_,
	.exit_dialog = ctr_lte_if_v2_exit_dialog_,
	.enter_data_mode = ctr_lte_if_v2_enter_data_mode_,
	.exit_data_mode = ctr_lte_if_v2_exit_data_mode_,
	.send_line = ctr_lte_if_v2_send_line_,
	.recv_line = ctr_lte_if_v2_recv_line_,
	.free_line = ctr_lte_if_v2_free_line_,
	.send_data = ctr_lte_if_v2_send_data_,
	.recv_data = ctr_lte_if_v2_recv_data_,
};

#define CTR_LTE_IF_V2_INIT(n)                                                                      \
	static const struct ctr_lte_if_v2_config inst_##n##_config = {                             \
		.uart_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                         \
		.reset_spec = GPIO_DT_SPEC_INST_GET(n, reset_gpios),                               \
		.wakeup_spec = GPIO_DT_SPEC_INST_GET(n, wakeup_gpios),                             \
	};                                                                                         \
	static struct ctr_lte_if_v2_data inst_##n##_data = {                                       \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_lte_if_v2_drv_init, NULL, &inst_##n##_data,                   \
			      &inst_##n##_config, POST_KERNEL, CONFIG_SERIAL_INIT_PRIORITY,        \
			      &ctr_lte_if_v2_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_LTE_IF_V2_INIT)
