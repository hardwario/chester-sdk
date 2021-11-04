#include <ctr_lte_uart.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_uart, LOG_LEVEL_DBG);

/* TODO Remove this after K_MEM_SLAB_DEFINE_STATIC definition makes it to mainline */
#define K_MEM_SLAB_DEFINE_STATIC(name, slab_block_size, slab_num_blocks, slab_align)               \
	static char __noinit __aligned(WB_UP(slab_align))                                          \
	        _k_mem_slab_buf_##name[(slab_num_blocks)*WB_UP(slab_block_size)];                  \
	static Z_STRUCT_SECTION_ITERABLE(k_mem_slab, name) = Z_MEM_SLAB_INITIALIZER(               \
	        name, _k_mem_slab_buf_##name, WB_UP(slab_block_size), slab_num_blocks)

#define RX_BLOCK_SIZE 64
#define RX_LINE_MAX_SIZE 1024
#define RX_RING_BUF_SIZE 512
#define RX_TIMEOUT 100
#define TX_LINE_MAX_SIZE 1024
#define TX_PREFIX ""
#define TX_PREFIX_LEN 0
#define TX_SUFFIX "\r"
#define TX_SUFFIX_LEN 1

static atomic_t m_enabled;
static atomic_t m_reset;
static atomic_t m_stop;
static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
static ctr_lte_recv_cb m_recv_cb;
K_MEM_SLAB_DEFINE_STATIC(m_rx_slab, RX_BLOCK_SIZE, 2, 4);
static K_MUTEX_DEFINE(m_mut);
static struct k_poll_signal m_rx_disabled_sig;
static struct k_poll_signal m_tx_done_sig;
static struct onoff_client m_cli;
static struct onoff_manager *m_mgr;
static struct ring_buf m_rx_ring_buf;
static uint8_t __aligned(4) m_rx_ring_buf_mem[RX_RING_BUF_SIZE];

static void rx_receive_work_handler(struct k_work *work)
{
	static bool synced = true;
	static char buf[RX_LINE_MAX_SIZE];
	static size_t len;

	if (atomic_get(&m_reset)) {
		LOG_DBG("Reset request");

		ring_buf_reset(&m_rx_ring_buf);

		synced = false;

		atomic_set(&m_reset, false);
	}

	for (char c; ring_buf_get(&m_rx_ring_buf, (uint8_t *)&c, 1) != 0;) {
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

				if (m_recv_cb != NULL) {
					m_recv_cb(buf);
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

static K_WORK_DEFINE(rx_receive_work, rx_receive_work_handler);

static void rx_restart_work_handler(struct k_work *work)
{
	int ret;
	uint8_t *buf;

	ret = k_mem_slab_alloc(&m_rx_slab, (void **)&buf, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		return;
	}

	ret = uart_rx_enable(m_dev, buf, RX_BLOCK_SIZE, RX_TIMEOUT);

	if (ret < 0) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&m_rx_slab, (void **)&buf);
		return;
	}

	atomic_set(&m_reset, true);

	ret = k_work_submit(&rx_receive_work);

	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static K_WORK_DEFINE(rx_restart_work, rx_restart_work_handler);

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	int ret;
	uint8_t *buf;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("Event `UART_TX_DONE`");
		k_poll_signal_raise(&m_tx_done_sig, 0);
		break;

	case UART_TX_ABORTED:
		LOG_WRN("Event `UART_TX_ABORTED`");
		k_poll_signal_raise(&m_tx_done_sig, 0);
		break;

	case UART_RX_RDY:
		LOG_DBG("Event `UART_RX_RDY`");

		if (evt->data.rx.len > 0) {
			struct uart_event_rx *rx = &evt->data.rx;

			if (ring_buf_put(&m_rx_ring_buf, rx->buf + rx->offset, rx->len) !=
			    rx->len) {
				atomic_set(&m_reset, true);
			}

			ret = k_work_submit(&rx_receive_work);

			if (ret < 0) {
				LOG_ERR("Call `k_work_submit` failed: %d", ret);
			}
		}

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("Event `UART_RX_BUF_REQUEST`");

		ret = k_mem_slab_alloc(&m_rx_slab, (void **)&buf, K_NO_WAIT);

		if (ret < 0) {
			LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
			break;
		}

		ret = uart_rx_buf_rsp(dev, buf, RX_BLOCK_SIZE);

		if (ret < 0) {
			LOG_ERR("Call `uart_rx_buf_rsp` failed: %d", ret);
			k_mem_slab_free(&m_rx_slab, (void **)&buf);
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("Event `UART_RX_BUF_RELEASED`");
		k_mem_slab_free(&m_rx_slab, (void **)&evt->data.rx_buf.buf);
		break;

	case UART_RX_DISABLED:
		LOG_DBG("Event `UART_RX_DISABLED`");

		if (atomic_get(&m_stop)) {
			k_poll_signal_raise(&m_rx_disabled_sig, 0);

		} else {
			ret = k_work_submit(&rx_restart_work);

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

int ctr_lte_uart_init(ctr_lte_recv_cb recv_cb)
{
	int ret;

	m_recv_cb = recv_cb;

	ring_buf_init(&m_rx_ring_buf, RX_RING_BUF_SIZE, m_rx_ring_buf_mem);

	k_poll_signal_init(&m_tx_done_sig);
	k_poll_signal_init(&m_rx_disabled_sig);

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	m_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);

	if (m_mgr == NULL) {
		LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
		return -ENXIO;
	}

	ret = uart_callback_set(m_dev, uart_callback, NULL);

	if (ret < 0) {
		LOG_ERR("Call `uart_callback_set` failed: %d", ret);
		return ret;
	}

	ret = pm_device_state_set(m_dev, PM_DEVICE_STATE_SUSPEND);

	if (ret < 0) {
		LOG_ERR("Call `pm_device_state_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int request_hfclk(void)
{
	int ret;

	static struct k_poll_signal sig;

	k_poll_signal_init(&sig);
	sys_notify_init_signal(&m_cli.notify, &sig);

	ret = onoff_request(m_mgr, &m_cli);

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

int ctr_lte_uart_enable(void)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (atomic_get(&m_enabled)) {
		k_mutex_unlock(&m_mut);
		return 0;
	}

	ret = request_hfclk();

	if (ret < 0) {
		LOG_ERR("Call `request_hfclk` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	ret = pm_device_state_set(m_dev, PM_DEVICE_STATE_ACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `pm_device_state_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_poll_signal_reset(&m_rx_disabled_sig);

	uint8_t *buf;

	ret = k_mem_slab_alloc(&m_rx_slab, (void **)&buf, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	atomic_set(&m_stop, false);

	ret = uart_rx_enable(m_dev, buf, RX_BLOCK_SIZE, RX_TIMEOUT);

	if (ret < 0) {
		LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
		k_mem_slab_free(&m_rx_slab, (void **)&buf);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	atomic_set(&m_enabled, true);

	k_mutex_unlock(&m_mut);
	return 0;
}

static int release_hfclk(void)
{
	int ret;

	ret = onoff_cancel_or_release(m_mgr, &m_cli);

	if (ret < 0) {
		LOG_ERR("Call `onoff_cancel_or_release` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_uart_disable(void)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!atomic_get(&m_enabled)) {
		k_mutex_unlock(&m_mut);
		return 0;
	}

	atomic_set(&m_stop, true);

	ret = uart_rx_disable(m_dev);

	if (ret < 0) {
		LOG_ERR("Call `uart_rx_disable` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
		                         &m_rx_disabled_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	ret = pm_device_state_set(m_dev, PM_DEVICE_STATE_SUSPEND);

	if (ret < 0) {
		LOG_ERR("Call `pm_device_state_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	ret = release_hfclk();

	if (ret < 0) {
		LOG_ERR("Call `release_hfclk` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	atomic_set(&m_enabled, false);

	k_mutex_unlock(&m_mut);
	return 0;
}

int ctr_lte_uart_send(const char *fmt, va_list ap)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!atomic_get(&m_enabled)) {
		k_mutex_unlock(&m_mut);
		return -EBUSY;
	}

	static char buf[TX_LINE_MAX_SIZE];

	ret = vsnprintf(&buf[TX_PREFIX_LEN], sizeof(buf) - TX_PREFIX_LEN - TX_SUFFIX_LEN, fmt, ap);

	if (ret < 0) {
		LOG_ERR("Call `vsnprintf` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return -EINVAL;

	} else if (ret >= sizeof(buf) - TX_PREFIX_LEN - TX_SUFFIX_LEN) {
		LOG_ERR("Buffer too small");
		k_mutex_unlock(&m_mut);
		return -ENOBUFS;
	}

	LOG_INF("TX: %s", &buf[TX_PREFIX_LEN]);

	size_t len = strlen(&buf[TX_PREFIX_LEN]);

	memcpy(&buf[0], TX_PREFIX, TX_PREFIX_LEN);
	len += TX_PREFIX_LEN;

	memcpy(&buf[len], TX_SUFFIX, TX_SUFFIX_LEN);
	len += TX_SUFFIX_LEN;

	k_poll_signal_reset(&m_tx_done_sig);

	ret = uart_tx(m_dev, buf, len, SYS_FOREVER_MS);

	if (ret < 0) {
		LOG_ERR("Call `uart_tx` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
		                         &m_tx_done_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}
