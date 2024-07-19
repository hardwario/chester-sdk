#ifndef CHESTER_INCLUDE_DRIVERS_CTR_LRW_LINK_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_LRW_LINK_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TX_LINE_BUF_SIZE 1024
#define RX_LINE_BUF_SIZE 1024

#define RX_HEAP_MEM_SIZE    4096
#define RX_PIPE_BUF_SIZE    512
#define RX_SLAB_BLOCK_ALIGN 4
#define RX_SLAB_BLOCK_COUNT 2
#define RX_SLAB_BLOCK_SIZE  64
#define RX_TIMEOUT          100000

enum ctr_lrw_link_event {
	CTR_LRW_LINK_EVENT_RESET = 0,
	CTR_LRW_LINK_EVENT_INDICATE = 1,
	CTR_LRW_LINK_EVENT_RX_LINE = 2,
	CTR_LRW_LINK_EVENT_RX_LOSS = 3,
};

struct ctr_lrw_link_data;

typedef void (*ctr_lrw_link_user_cb)(const struct device *dev, enum ctr_lrw_link_event event,
				     void *user_data);

typedef int (*ctr_lrw_link_api_set_callback)(const struct device *dev, ctr_lrw_link_user_cb user_cb,
					     void *user_data);
typedef void (*ctr_lrw_link_api_lock)(const struct device *dev);
typedef void (*ctr_lrw_link_api_unlock)(const struct device *dev);
typedef int (*ctr_lrw_link_api_reset)(const struct device *dev);
typedef int (*ctr_lrw_link_api_enable_uart)(const struct device *dev);
typedef int (*ctr_lrw_link_api_disable_uart)(const struct device *dev);
typedef int (*ctr_lrw_link_api_enter_dialog)(const struct device *dev);
typedef int (*ctr_lrw_link_api_exit_dialog)(const struct device *dev);
typedef int (*ctr_lrw_link_api_send_line)(const struct device *dev, k_timeout_t timeout,
					  const char *format, va_list ap);
typedef int (*ctr_lrw_link_api_recv_line)(const struct device *dev, k_timeout_t timeout,
					  char **line);
typedef int (*ctr_lrw_link_api_free_line)(const struct device *dev, char *line);

struct ctr_lrw_link_data {
	atomic_t in_dialog;
	atomic_t rx_line_synced;
	atomic_t recv_line;
	atomic_t stop_request;
	bool enabled;
	char rx_line_buf[RX_LINE_BUF_SIZE];
	char rx_slab_mem[RX_SLAB_BLOCK_SIZE * RX_SLAB_BLOCK_COUNT] __aligned(RX_SLAB_BLOCK_ALIGN);
	char tx_line_buf[TX_LINE_BUF_SIZE];
	const struct device *dev;
	ctr_lrw_link_user_cb user_cb;
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

struct ctr_lrw_link_driver_api {
	ctr_lrw_link_api_set_callback set_callback;
	ctr_lrw_link_api_lock lock;
	ctr_lrw_link_api_unlock unlock;
	ctr_lrw_link_api_reset reset;
	ctr_lrw_link_api_enable_uart enable_uart;
	ctr_lrw_link_api_disable_uart disable_uart;
	ctr_lrw_link_api_enter_dialog enter_dialog;
	ctr_lrw_link_api_exit_dialog exit_dialog;
	ctr_lrw_link_api_send_line send_line;
	ctr_lrw_link_api_recv_line recv_line;
	ctr_lrw_link_api_free_line free_line;
};

static inline int ctr_lrw_link_set_callback(const struct device *dev, ctr_lrw_link_user_cb user_cb,
					    void *user_data)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->set_callback(dev, user_cb, user_data);
}

static inline void ctr_lrw_link_lock(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	api->lock(dev);
}

static inline void ctr_lrw_link_unlock(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	api->unlock(dev);
}

static inline int ctr_lrw_link_reset(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->reset(dev);
}

static inline int ctr_lrw_link_enable_uart(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->enable_uart(dev);
}

static inline int ctr_lrw_link_disable_uart(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->disable_uart(dev);
}

static inline int ctr_lrw_link_enter_dialog(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->enter_dialog(dev);
}

static inline int ctr_lrw_link_exit_dialog(const struct device *dev)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->exit_dialog(dev);
}

static inline int ctr_lrw_link_send_line(const struct device *dev, k_timeout_t timeout,
					 const char *format, ...)
{
	int ret;

	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	va_list ap;
	va_start(ap, format);
	ret = api->send_line(dev, timeout, format, ap);
	va_end(ap);

	return ret;
}

static inline int ctr_lrw_link_recv_line(const struct device *dev, k_timeout_t timeout, char **line)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->recv_line(dev, timeout, line);
}

static inline int ctr_lrw_link_free_line(const struct device *dev, char *line)
{
	const struct ctr_lrw_link_driver_api *api =
		(const struct ctr_lrw_link_driver_api *)dev->api;

	return api->free_line(dev, line);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_LRW_LINK_H_ */
