/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_V2_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_V2_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_if_v2_event {
	CTR_LTE_IF_V2_EVENT_RESET = 0,
	CTR_LTE_IF_V2_EVENT_INDICATE = 1,
	CTR_LTE_IF_V2_EVENT_RX_LINE = 2,
	CTR_LTE_IF_V2_EVENT_RX_DATA = 3,
	CTR_LTE_IF_V2_EVENT_RX_LOSS = 4,
};

typedef void (*ctr_lte_if_v2_user_cb)(const struct device *dev, enum ctr_lte_if_v2_event event,
				      void *user_data);

typedef int (*ctr_lte_if_v2_api_set_callback)(const struct device *dev,
					      ctr_lte_if_v2_user_cb user_cb, void *user_data);
typedef void (*ctr_lte_if_v2_api_lock)(const struct device *dev);
typedef void (*ctr_lte_if_v2_api_unlock)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_reset)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_wake_up)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_enable_uart)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_disable_uart)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_enter_dialog)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_exit_dialog)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_enter_data_mode)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_exit_data_mode)(const struct device *dev);
typedef int (*ctr_lte_if_v2_api_send_line)(const struct device *dev, k_timeout_t timeout,
					   const char *format, va_list ap);
typedef int (*ctr_lte_if_v2_api_recv_line)(const struct device *dev, k_timeout_t timeout,
					   char **line);
typedef int (*ctr_lte_if_v2_api_free_line)(const struct device *dev, char *line);
typedef int (*ctr_lte_if_v2_api_send_data)(const struct device *dev, k_timeout_t timeout,
					   const void *buf, size_t len);
typedef int (*ctr_lte_if_v2_api_recv_data)(const struct device *dev, k_timeout_t timeout, void *buf,
					   size_t size, size_t *len);

struct ctr_lte_if_v2_driver_api {
	ctr_lte_if_v2_api_set_callback set_callback;
	ctr_lte_if_v2_api_lock lock;
	ctr_lte_if_v2_api_unlock unlock;
	ctr_lte_if_v2_api_reset reset;
	ctr_lte_if_v2_api_wake_up wake_up;
	ctr_lte_if_v2_api_enable_uart enable_uart;
	ctr_lte_if_v2_api_disable_uart disable_uart;
	ctr_lte_if_v2_api_enter_dialog enter_dialog;
	ctr_lte_if_v2_api_exit_dialog exit_dialog;
	ctr_lte_if_v2_api_enter_data_mode enter_data_mode;
	ctr_lte_if_v2_api_exit_data_mode exit_data_mode;
	ctr_lte_if_v2_api_send_line send_line;
	ctr_lte_if_v2_api_recv_line recv_line;
	ctr_lte_if_v2_api_free_line free_line;
	ctr_lte_if_v2_api_send_data send_data;
	ctr_lte_if_v2_api_recv_data recv_data;
};

static inline int ctr_lte_if_v2_set_callback(const struct device *dev,
					     ctr_lte_if_v2_user_cb user_cb, void *user_data)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->set_callback(dev, user_cb, user_data);
}

static inline void ctr_lte_if_v2_lock(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	api->lock(dev);
}

static inline void ctr_lte_if_v2_unlock(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	api->unlock(dev);
}

static inline int ctr_lte_if_v2_reset(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->reset(dev);
}

static inline int ctr_lte_if_v2_wake_up(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->wake_up(dev);
}

static inline int ctr_lte_if_v2_enable_uart(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->enable_uart(dev);
}

static inline int ctr_lte_if_v2_disable_uart(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->disable_uart(dev);
}

static inline int ctr_lte_if_v2_enter_dialog(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->enter_dialog(dev);
}

static inline int ctr_lte_if_v2_exit_dialog(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->exit_dialog(dev);
}

static inline int ctr_lte_if_v2_enter_data_mode(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->enter_data_mode(dev);
}

static inline int ctr_lte_if_v2_exit_data_mode(const struct device *dev)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->exit_data_mode(dev);
}

static inline int ctr_lte_if_v2_send_line(const struct device *dev, k_timeout_t timeout,
					  const char *format, ...)
{
	int ret;

	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	va_list ap;
	va_start(ap, format);
	ret = api->send_line(dev, timeout, format, ap);
	va_end(ap);

	return ret;
}

static inline int ctr_lte_if_v2_recv_line(const struct device *dev, k_timeout_t timeout,
					  char **line)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->recv_line(dev, timeout, line);
}

static inline int ctr_lte_if_v2_free_line(const struct device *dev, char *line)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->free_line(dev, line);
}

static inline int ctr_lte_if_v2_send_data(const struct device *dev, k_timeout_t timeout,
					  const void *buf, size_t len)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->send_data(dev, timeout, buf, len);
}

static inline int ctr_lte_if_v2_recv_data(const struct device *dev, k_timeout_t timeout, void *buf,
					  size_t size, size_t *len)
{
	const struct ctr_lte_if_v2_driver_api *api =
		(const struct ctr_lte_if_v2_driver_api *)dev->api;

	return api->recv_data(dev, timeout, buf, size, len);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_V2_H_ */
