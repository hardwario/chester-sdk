/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "mock.h"
/* CHESTER includes */
#include <chester/drivers/ctr_lte_link.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DT_DRV_COMPAT hardwario_ctr_lte_link

LOG_MODULE_REGISTER(ctr_lte_link, LOG_LEVEL_INF);

#define RESET_DELAY   K_MSEC(100)
#define RESET_PAUSE   K_SECONDS(3)
#define WAKE_UP_DELAY K_MSEC(100)
#define WAKE_UP_PAUSE K_MSEC(100)
#define TX_GUARD_TIME K_MSEC(10)

#define TX_LINE_SUFFIX "\r\n"

#define TX_LINE_BUF_SIZE 1024
#define RX_LINE_BUF_SIZE 1024
#define RX_HEAP_MEM_SIZE 4096
#define RX_PIPE_BUF_SIZE 512
#define STACK_SIZE       1024
#define WORKQ_PRIORITY   K_PRIO_COOP(7)

#define VIOLATION_BUF_SIZE 256

struct ctr_lte_link_data {
	bool enabled;
	struct k_mutex lock;
	atomic_t in_dialog;
	atomic_t in_data_mode;
	atomic_t stop_request;
	char rx_line_buf[RX_LINE_BUF_SIZE];
	char tx_line_buf[TX_LINE_BUF_SIZE];
	const struct device *dev;
	ctr_lte_link_user_cb user_cb;
	void *user_data;
	struct k_work event_dispatch_work;
	struct k_work_delayable urc_dispatch_work;

	struct k_work_q workq;
	K_KERNEL_STACK_MEMBER(workq_stack, STACK_SIZE);
	struct k_fifo rx_fifo;
	struct k_heap rx_heap;
	uint8_t rx_heap_mem[RX_HEAP_MEM_SIZE];
	k_timepoint_t tx_timepoint;

	/* The script state is shared by the FSM thread (TX lines) and the mock work queue (URCs);
	 * script_lock guards it, so the test thread can read it while the FSM sits in a dialog */
	struct k_spinlock script_lock;
	struct mock_link_item *items;
	size_t items_count;
	size_t items_index;
	int violations;
	char violation[VIOLATION_BUF_SIZE];

	const char *pending_rx_data;
	size_t pending_rx_data_len;
};

static inline struct ctr_lte_link_data *get_data(const struct device *dev)
{
	return dev->data;
}

/* Record a departure from the script. Asserting here would fail the test from the FSM thread or
 * from the mock work queue, so the first violation is kept for mock_ctr_lte_link_assert_done(),
 * which asserts from the test thread. The caller holds script_lock. */
static void record_violation(struct ctr_lte_link_data *data, const char *format, ...)
{
	char msg[VIOLATION_BUF_SIZE];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	LOG_ERR("Script violation: %s", msg);

	if (!data->violations++) {
		strcpy(data->violation, msg);
	}
}

static int ctr_lte_link_set_callback_(const struct device *dev, ctr_lte_link_user_cb user_cb,
				      void *user_data)
{

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void ctr_lte_link_lock_(const struct device *dev)
{
	LOG_DBG("Lock");
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);
}

static void ctr_lte_link_unlock_(const struct device *dev)
{
	LOG_DBG("Unlock");
	k_mutex_unlock(&get_data(dev)->lock);
}

static int ctr_lte_link_reset_(const struct device *dev)
{

	LOG_DBG("Reset");
	k_sleep(RESET_DELAY);
	// k_sleep(RESET_PAUSE);
	return 0;
}

static int rx(const struct device *dev, const char line[])
{
	size_t len = strlen(line);

	struct ctr_lte_link_data *data = get_data(dev);

	char *p = k_heap_alloc(&data->rx_heap, len + 1, K_NO_WAIT);
	if (!p) {
		LOG_ERR("Call `k_heap_alloc` failed");
		if (data->user_cb) {
			data->user_cb(data->dev, CTR_LTE_LINK_EVENT_RX_LOSS, data->user_data);
		}

		return -ENOMEM;
	}

	LOG_DBG("Allocated from heap: %p - %u byte(s)", (void *)p, len + 1);

	strcpy(p, line);

	int ret = k_fifo_alloc_put(&data->rx_fifo, p);
	if (ret) {
		LOG_ERR("Call `k_fifo_alloc_put` failed: %d", ret);
		k_heap_free(&data->rx_heap, p);
		LOG_DBG("Released from heap: %p", (void *)p);
		return -ENOMEM;
	}

	if (data->user_cb && !atomic_get(&data->in_dialog)) {
		k_work_submit_to_queue(&data->workq, &data->event_dispatch_work);
	}

	return 0;
}

static void event_dispatch_work_handler(struct k_work *work)
{
	struct ctr_lte_link_data *const data =
		CONTAINER_OF(work, struct ctr_lte_link_data, event_dispatch_work);

	if (data->user_cb) {
		data->user_cb(data->dev, CTR_LTE_LINK_EVENT_RX_LINE, data->user_data);
	}
}

/* Schedule the next item if it is a URC; the caller holds script_lock */
static void schedule_next_urc(struct ctr_lte_link_data *data)
{
	if (data->items_index < data->items_count) {
		const struct mock_link_item *item = &data->items[data->items_index];
		if (item->tx == NULL) {
			k_work_schedule(&data->urc_dispatch_work, K_MSEC(item->delay_ms));
		}
	}
}

static void urc_dispatch_work_handler(struct k_work *work)
{
	struct ctr_lte_link_data *const data = CONTAINER_OF(
		(struct k_work_delayable *)work, struct ctr_lte_link_data, urc_dispatch_work);

	const struct mock_link_item *item = NULL;

	k_spinlock_key_t key = k_spin_lock(&data->script_lock);

	if (data->items_index < data->items_count && data->items[data->items_index].tx == NULL) {
		item = &data->items[data->items_index++];

		/* If URC starts with #XRECV:, store rx2 as pending data */
		if (item->rx && strncmp(item->rx, "#XRECV:", 7) == 0 && item->rx2) {
			int expected_len = 0;
			sscanf(item->rx + 7, " %d", &expected_len);
			if ((size_t)expected_len != strlen(item->rx2)) {
				record_violation(data,
						 "XRECV len mismatch: header says %d, data is %zu",
						 expected_len, strlen(item->rx2));
			}

			data->pending_rx_data = item->rx2;
			data->pending_rx_data_len = strlen(item->rx2);
		}

		schedule_next_urc(data);
	}

	k_spin_unlock(&data->script_lock, key);

	if (item && item->rx) {
		rx(data->dev, item->rx);
	}
}

static int ctr_lte_link_wake_up_(const struct device *dev)
{

	LOG_INF("Wake up");
	k_sleep(WAKE_UP_DELAY);
	// k_sleep(WAKE_UP_PAUSE);
	rx(dev, "Ready");
	return 0;
}

static int ctr_lte_link_enable_uart_(const struct device *dev)
{

	LOG_DBG("Enable UART");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	atomic_set(&get_data(dev)->in_dialog, false);
	atomic_set(&get_data(dev)->in_data_mode, false);
	atomic_set(&get_data(dev)->stop_request, false);

	get_data(dev)->enabled = true;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_disable_uart_(const struct device *dev)
{

	LOG_DBG("Disable UART");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	get_data(dev)->enabled = false;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_enter_data_mode_(const struct device *dev)
{
	LOG_DBG("Enter data mode");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_data_mode, true);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_exit_data_mode_(const struct device *dev)
{
	LOG_DBG("Exit data mode");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_data_mode, false);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_enter_dialog_(const struct device *dev)
{
	LOG_DBG("Enter dialog");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_dialog, true);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_exit_dialog_(const struct device *dev)
{
	LOG_DBG("Exit dialog");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	atomic_set(&get_data(dev)->in_dialog, false);

	k_mutex_unlock(&get_data(dev)->lock);

	/* As the driver does: the URCs that came during the dialog are dispatched now */
	if (get_data(dev)->user_cb) {
		k_work_submit_to_queue(&get_data(dev)->workq, &get_data(dev)->event_dispatch_work);
	}

	return 0;
}

static int ctr_lte_link_send_line_(const struct device *dev, k_timeout_t timeout,
				   const char *format, va_list ap)
{

	struct ctr_lte_link_data *data = get_data(dev);

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->enabled) {
		k_mutex_unlock(&data->lock);
		return -EBUSY;
	}

	if (data->in_data_mode) {
		k_mutex_unlock(&data->lock);
		return -EPERM;
	}

	// if (!sys_timepoint_expired(data->tx_timepoint)) {
	// 	k_sleep(sys_timepoint_timeout(data->tx_timepoint));
	// }

	size_t size = sizeof(data->tx_line_buf) - strlen(TX_LINE_SUFFIX);

	int ret = vsnprintf(data->tx_line_buf, size, format, ap);
	if (ret < 0) {
		LOG_ERR("Call `vsnprintf` failed: %d", ret);
		k_mutex_unlock(&data->lock);
		return -EINVAL;
	} else if (ret >= size) {
		LOG_ERR("Buffer too small");
		k_mutex_unlock(&data->lock);
		return -ENOBUFS;
	}

	LOG_INF("Send line: %s", data->tx_line_buf);

	// strcat(data->tx_line_buf, TX_LINE_SUFFIX);

	/* A line the script does not expect gets no response and is recorded as a violation */
	const struct mock_link_item *item = NULL;

	k_spinlock_key_t key = k_spin_lock(&data->script_lock);

	if (data->items_index >= data->items_count) {
		record_violation(data, "unexpected TX after the end of the script: %s",
				 data->tx_line_buf);
	} else if (data->items[data->items_index].tx == NULL) {
		record_violation(data, "unexpected TX at item %zu, expected URC %s: %s",
				 data->items_index, data->items[data->items_index].rx,
				 data->tx_line_buf);
	} else if (strcmp(data->tx_line_buf, data->items[data->items_index].tx) != 0) {
		record_violation(data, "unexpected TX at item %zu, expected %s: %s",
				 data->items_index, data->items[data->items_index].tx,
				 data->tx_line_buf);
	} else {
		item = &data->items[data->items_index++];
	}

	k_spin_unlock(&data->script_lock, key);

	if (item) {
		if (item->rx) {
			rx(dev, item->rx);
		}
		if (item->rx2) {
			rx(dev, item->rx2);
		}

		/* Only after the response, so that a URC due at once comes after it */
		key = k_spin_lock(&data->script_lock);
		schedule_next_urc(data);
		k_spin_unlock(&data->script_lock, key);
	}

	// if (strcmp(data->tx_line_buf, "AT") == 0) {
	// 	rx(dev, "OK");
	// }
	// if (strcmp(data->tx_line_buf, "AT+CGSN=1") == 0) {
	// 	rx(dev, "+CGSN: \"351358815178345\"");
	// 	rx(dev, "OK");
	// }

	// data->tx_timepoint = sys_timepoint_calc(TX_GUARD_TIME);

	k_mutex_unlock(&data->lock);

	return 0;
}

static int ctr_lte_link_recv_line_(const struct device *dev, k_timeout_t timeout, char **line)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

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

static int ctr_lte_link_free_line_(const struct device *dev, char *line)
{

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	k_heap_free(&get_data(dev)->rx_heap, line);
	LOG_DBG("Released from heap: %p", (void *)line);

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_send_data_(const struct device *dev, k_timeout_t timeout, const void *buf,
				   size_t len)
{

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->enabled) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EBUSY;
	}

	if (!get_data(dev)->in_data_mode) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EPERM;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_lte_link_recv_data_(const struct device *dev, k_timeout_t timeout, void *buf,
				   size_t size, size_t *len)
{
	struct ctr_lte_link_data *data = get_data(dev);

	LOG_INF("size: %d pending: %d", size, data->pending_rx_data_len);

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->enabled) {
		k_mutex_unlock(&data->lock);
		return -EBUSY;
	}

	if (!data->in_data_mode) {
		k_mutex_unlock(&data->lock);
		return -EPERM;
	}

	*len = 0;

	if (data->pending_rx_data && data->pending_rx_data_len > 0) {
		size_t copy_len = data->pending_rx_data_len;
		if (copy_len > size) {
			copy_len = size;
		}
		memcpy(buf, data->pending_rx_data, copy_len);
		*len = copy_len;

		data->pending_rx_data = NULL;
		data->pending_rx_data_len = 0;
		rx(dev, "OK"); // Simulate OK after XRECV data is read
	}

	k_mutex_unlock(&data->lock);

	return 0;
}

static int ctr_lte_link_drv_init(const struct device *dev)
{
	struct ctr_lte_link_data *data = get_data(dev);
	k_mutex_init(&data->lock);
	k_fifo_init(&data->rx_fifo);
	k_heap_init(&data->rx_heap, data->rx_heap_mem, RX_HEAP_MEM_SIZE);

	struct k_work_queue_config cfg = {
		.name = "ctr_lte_link_drv_workq",
	};

	k_work_queue_start(&data->workq, data->workq_stack,
			   K_THREAD_STACK_SIZEOF(data->workq_stack), WORKQ_PRIORITY, &cfg);

	k_work_init(&data->event_dispatch_work, event_dispatch_work_handler);
	k_work_init_delayable(&data->urc_dispatch_work, urc_dispatch_work_handler);
	return 0;
}

static const struct ctr_lte_link_driver_api ctr_lte_link_driver_api = {
	.set_callback = ctr_lte_link_set_callback_,
	.lock = ctr_lte_link_lock_,
	.unlock = ctr_lte_link_unlock_,
	.reset = ctr_lte_link_reset_,
	.wake_up = ctr_lte_link_wake_up_,
	.enable_uart = ctr_lte_link_enable_uart_,
	.disable_uart = ctr_lte_link_disable_uart_,
	.enter_dialog = ctr_lte_link_enter_dialog_,
	.exit_dialog = ctr_lte_link_exit_dialog_,
	.enter_data_mode = ctr_lte_link_enter_data_mode_,
	.exit_data_mode = ctr_lte_link_exit_data_mode_,
	.send_line = ctr_lte_link_send_line_,
	.recv_line = ctr_lte_link_recv_line_,
	.free_line = ctr_lte_link_free_line_,
	.send_data = ctr_lte_link_send_data_,
	.recv_data = ctr_lte_link_recv_data_,
};

#define CTR_LTE_LINK_INIT(n)                                                                       \
	static struct ctr_lte_link_data inst_##n##_data = {                                        \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_lte_link_drv_init, NULL, &inst_##n##_data, NULL, POST_KERNEL, \
			      CONFIG_SERIAL_INIT_PRIORITY, &ctr_lte_link_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_LTE_LINK_INIT)

void mock_ctr_lte_link_start(struct mock_link_item *items, size_t count)
{
	LOG_INF("Mock CTR LTE LINK start");

	const struct device *dev = DEVICE_DT_INST_GET(0);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device is not ready");
		return;
	}

	struct ctr_lte_link_data *data = get_data(DEVICE_DT_INST_GET(0));

	k_work_cancel_delayable(&data->urc_dispatch_work);

	k_spinlock_key_t key = k_spin_lock(&data->script_lock);

	data->items = items;
	data->items_count = count;
	data->items_index = 0;
	data->violations = 0;
	data->violation[0] = '\0';

	/* A script may start with a URC */
	schedule_next_urc(data);

	k_spin_unlock(&data->script_lock, key);
}

void mock_ctr_lte_link_assert_done(void)
{
	struct ctr_lte_link_data *data = get_data(DEVICE_DT_INST_GET(0));

	int violations;
	char violation[VIOLATION_BUF_SIZE];
	size_t index;
	size_t count;
	const char *next_tx = NULL;
	const char *next_rx = NULL;

	k_spinlock_key_t key = k_spin_lock(&data->script_lock);

	violations = data->violations;
	strcpy(violation, data->violation);
	index = data->items_index;
	count = data->items_count;
	if (index < count) {
		next_tx = data->items[index].tx;
		next_rx = data->items[index].rx;
	}

	k_spin_unlock(&data->script_lock, key);

	zassert_equal(violations, 0, "%d script violation(s), the first: %s", violations,
		      violation);
	zassert_equal(index, count, "script stopped at item %zu of %zu (tx: %s, rx: %s)", index,
		      count, next_tx ? next_tx : "-", next_rx ? next_rx : "-");
}
