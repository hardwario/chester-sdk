#include <hio_lte_uart.h>
#include <hio_lte_talk.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_sys.h>
#include <hio_test.h>
#include <hio_util.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <zephyr.h>

// Standard includes
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TX_LINE_MAX_SIZE 1024
#define RX_LINE_MAX_SIZE 1024

#define RX_TIMEOUT 10

HIO_LOG_REGISTER(hio_lte_uart, HIO_LOG_LEVEL_DBG);

// Handle for UART
static const struct device *dev;

// Receive interrupt double buffer
static uint8_t __aligned(4) rx_buffer[2][256];

// Ring buffer object for received characters
static hio_sys_rbuf_t rx_ring_buf;

// Ring buffer memory for received characters
static uint8_t __aligned(4) rx_ring_buf_mem[1024];

// Heap object for received line buffers
static hio_sys_heap_t rx_heap;

// Heap memory for received line buffers
static char __aligned(4) rx_heap_mem[4 * RX_LINE_MAX_SIZE];

// Message queue object for pointers to received lines
static hio_sys_msgq_t rx_msgq;

// Message queue memory for pointers to received lines
static char __aligned(4) rx_msgq_mem[64 * sizeof(char *)];

// Semaphore indicating finished transmission
static hio_sys_sem_t tx_sem;

// Semaphore indicating received characters
static hio_sys_sem_t rx_sem;

// Task object for receiver worker
static hio_sys_task_t rx_task;

// Task memory for receiver worker
static HIO_SYS_TASK_STACK_DEFINE(rx_task_stack, 2048);

uint8_t *next_buf;

static struct onoff_client hfclk_cli;

static bool enabled;

static struct k_poll_signal rx_disabled_sig;

static void
uart_callback(const struct device *dev,
              struct uart_event *evt, void *user_data)
{
    uint8_t *p;

    switch (evt->type) {
    case UART_TX_DONE:
        hio_sys_sem_give(&tx_sem);
        break;
    case UART_TX_ABORTED:
        hio_log_wrn("Sending timed out or aborted");
        hio_sys_sem_give(&tx_sem);
        break;
    case UART_RX_RDY:
        p = evt->data.rx.buf + evt->data.rx.offset;
        hio_sys_rbuf_put(&rx_ring_buf, p, evt->data.rx.len);
        hio_sys_sem_give(&rx_sem);
        break;
    case UART_RX_BUF_REQUEST:
        uart_rx_buf_rsp(dev, next_buf, sizeof(rx_buffer[0]));
        break;
	case UART_RX_BUF_RELEASED:
		next_buf = evt->data.rx_buf.buf;
        break;
    case UART_RX_DISABLED:
        k_poll_signal_raise(&rx_disabled_sig, 0);
        break;
    case UART_RX_STOPPED:
        hio_log_wrn("Receiving stopped");
        // TODO Handle this
        break;
    }
}

static void
process_rsp(const char *buf, size_t len)
{
    // Allocate buffer from heap
    char *p = hio_sys_heap_alloc(&rx_heap, len + 1, HIO_SYS_NO_WAIT);

    // Memory allocation failed?
    if (p == NULL) {
        hio_log_err("Call `hio_sys_heap_alloc` failed");

    } else {
        // Copy line to allocated buffer
        memcpy(p, buf, len);
        p[len] = '\0';

        // Store pointer to buffer to RX queue
        if (hio_sys_msgq_put(&rx_msgq, &p, HIO_SYS_NO_WAIT) < 0) {
            hio_log_err("Call `hio_sys_msgq_put` failed");
        }
    }
}

static bool
starts_with(const char *s, const char *pfx)
{
    return strncmp(s, pfx, strlen(pfx)) == 0 ? true : false;
}

static void
process_urc(const char *buf, size_t len)
{
    if (starts_with(buf, "+CEREG: ")) {
        hio_lte_talk_cereg(buf);
        return;
    }
}

static void
process_rx_char(char c)
{
    // Buffer for received characters
    static char buf[RX_LINE_MAX_SIZE];

    // Number of received characters
    static size_t len;

    // Indicates buffer is clipped
    static bool clipped;

    // Received new line character?
    if (c == '\r' || c == '\n') {

        // If buffer is not clipped and contains something...
        if (!clipped && len > 0) {
            if (hio_test_is_active()) {
                hio_log_dbg("RSP: %s", buf);

            } else if (strcmp(buf, "Ready") != 0) {
                if (buf[0] != '+' && buf[0] != '%') {
                    hio_log_dbg("RSP: %s", buf);
                    process_rsp(buf, len);

                } else {
                    hio_log_dbg("URC: %s", buf);
                    process_urc(buf, len);
                }
            }
        }

        // Reset line
        len = 0;

        // Reset clip indicator
        clipped = false;

    } else {
        // Check for insufficient room...
        if (len >= sizeof(buf) - 1) {

            // Indicate clipped buffer
            clipped = true;

        } else {
            // Store character to buffer
            buf[len++] = c;
            buf[len] = '\0';
        }
    }
}

static void
rx_task_entry(void *param)
{
    for (;;) {
        // Wait for semaphore...
        if (hio_sys_sem_take(&rx_sem, HIO_SYS_FOREVER) < 0) {
            // TODO Should never get here
            continue;
        }

        // Process all available characters in ring buffer
        for (char c; hio_sys_rbuf_get(&rx_ring_buf, &c, 1) > 0;) {
            process_rx_char(c);
        }
    }
}

int
hio_lte_uart_init(void)
{
    // Initialize RX ring buffer
    hio_sys_rbuf_init(&rx_ring_buf,
                      rx_ring_buf_mem, sizeof(rx_ring_buf_mem));

    // Initialize heap
    hio_sys_heap_init(&rx_heap, rx_heap_mem, sizeof(rx_heap_mem));

    // Initialize RX queue
    hio_sys_msgq_init(&rx_msgq, rx_msgq_mem, sizeof(char *), 64);

    hio_sys_sem_init(&tx_sem, 0);
    hio_sys_sem_init(&rx_sem, 0);

    hio_sys_task_init(&rx_task, "hio_lte_uart_rx",
                      rx_task_stack, HIO_SYS_TASK_STACK_SIZEOF(rx_task_stack),
                      rx_task_entry, NULL);

    k_poll_signal_init(&rx_disabled_sig);

    dev = device_get_binding("UART_0");

    if (dev == NULL) {
        hio_log_fat("Call `device_get_binding` failed");
        return -1;
    }

    struct uart_config cfg = {
        .baudrate = 115200,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
    };

    if (uart_configure(dev, &cfg) < 0) {
        hio_log_fat("Call `uart_configure` failed");
        return -2;
    }

    if (uart_callback_set(dev, uart_callback, NULL) < 0) {
        hio_log_fat("Call `uart_callback_set` failed");
        return -3;
    }

	if (pm_device_state_set(dev, PM_DEVICE_STATE_OFF, NULL, NULL) < 0) {
        hio_log_fat("Call `pm_device_state_set` failed");
        return -4;
    }

    return 0;
}

static int
request_hfclk(void)
{
    int ret;

    struct onoff_manager *mgr =
        z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);

    if (mgr == NULL) {
        LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
        return -ENXIO;
    }

    static struct k_poll_signal sig;

    k_poll_signal_init(&sig);
    sys_notify_init_signal(&hfclk_cli.notify, &sig);

    ret = onoff_request(mgr, &hfclk_cli);

    if (ret < 0) {
        LOG_ERR("Call `onoff_request` failed: %d", ret);
        return ret;
    }

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    return 0;
}

static int
release_hfclk(void)
{
    int ret;

    struct onoff_manager *mgr =
        z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);

    if (mgr == NULL) {
        LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
        return -ENXIO;
    }

    ret = onoff_cancel_or_release(mgr, &hfclk_cli);

    if (ret < 0) {
        LOG_ERR("Call `onoff_cancel_or_release` failed: %d", ret);
        return ret;
    }

    return 0;
}

int
hio_lte_uart_enable(void)
{
    int ret;

    if (enabled) {
        return 0;
    }

    ret = request_hfclk();

    if (ret < 0) {
        LOG_ERR("Call `request_hfclk` failed: %d", ret);
        return ret;
    }

    ret = pm_device_state_set(dev, PM_DEVICE_STATE_ACTIVE, NULL, NULL);

	if (ret < 0) {
        LOG_ERR("Call `pm_device_state_set` failed: %d", ret);
        return ret;
    }

    k_poll_signal_reset(&rx_disabled_sig);

    next_buf = rx_buffer[1];

    ret = uart_rx_enable(dev, rx_buffer[0], sizeof(rx_buffer[0]), RX_TIMEOUT);

    if (ret < 0) {
        LOG_ERR("Call `uart_rx_enable` failed: %d", ret);
        return ret;
    }

    enabled = true;

    return 0;
}

int
hio_lte_uart_disable(void)
{
    int ret;

    if (!enabled) {
        return 0;
    }

    ret = uart_rx_disable(dev);

    if (ret < 0) {
        LOG_ERR("Call `uart_rx_disable` failed: %d", ret);
        return ret;
    }

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &rx_disabled_sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    ret = pm_device_state_set(dev, PM_DEVICE_STATE_OFF, NULL, NULL);

	if (ret < 0) {
        LOG_ERR("Call `pm_device_state_set` failed: %d", ret);
        return ret;
    }

    ret = release_hfclk();

    if (ret < 0) {
        LOG_ERR("Call `release_hfclk` failed: %d", ret);
        return ret;
    }

    enabled = false;

    return 0;
}

int
hio_lte_uart_send(const char *fmt, va_list ap)
{
    static uint8_t __aligned(4) buf[TX_LINE_MAX_SIZE];

    int ret = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);

    if (ret < 0) {
        hio_log_err("Call `vsnprintf` failed");
        return -1;

    } else if (ret > sizeof(buf) - 2) {
        hio_log_err("Buffer too small");
        return -2;
    }

    hio_log_dbg("CMD: %s", buf);

    strcat(buf, "\r\n");

    if (uart_tx(dev, buf, strlen(buf), SYS_FOREVER_MS) < 0) {
        hio_log_err("Call `uart_tx` failed");
        return -3;
    }

    // TODO Shall we set some reasonable timeout?
    if (hio_sys_sem_take(&tx_sem, HIO_SYS_FOREVER) < 0) {
        hio_log_err("Call `hio_sys_sem_take` failed");
        return -4;
    }

    return ret;
}

int
hio_lte_uart_recv(char **s, int64_t timeout)
{
    int ret;

    static uint8_t __aligned(4) buf[RX_LINE_MAX_SIZE];

    char *p;

    ret = hio_sys_msgq_get(&rx_msgq, &p, timeout);

    if (ret < 0) {
        hio_log_err("Call `hio_sys_msgq_get` failed: %d", ret);
        *s = NULL;
        return -ETIMEDOUT;
    }

    strcpy(buf, p);

    hio_sys_heap_free(&rx_heap, p);

    *s = buf;

    return 0;
}
