/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_H_

#include <zephyr/device.h>

/* Standard includes */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ctr_lte_recv_cb)(const char *s);

typedef int (*ctr_lte_if_api_init)(const struct device *dev, ctr_lte_recv_cb recv_cb);
typedef int (*ctr_lte_if_api_reset)(const struct device *dev);
typedef int (*ctr_lte_if_api_wakeup)(const struct device *dev);
typedef int (*ctr_lte_if_api_enable)(const struct device *dev);
typedef int (*ctr_lte_if_api_disable)(const struct device *dev);
typedef int (*ctr_lte_if_api_send)(const struct device *dev, const char *fmt, va_list ap);
typedef int (*ctr_lte_if_api_send_raw)(const struct device *dev, const void *buf, size_t len);

struct ctr_lte_if_driver_api {
	ctr_lte_if_api_init init;
	ctr_lte_if_api_reset reset;
	ctr_lte_if_api_wakeup wakeup;
	ctr_lte_if_api_enable enable;
	ctr_lte_if_api_disable disable;
	ctr_lte_if_api_send send;
	ctr_lte_if_api_send_raw send_raw;
};

static inline int ctr_lte_if_init(const struct device *dev, ctr_lte_recv_cb recv_cb)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->init(dev, recv_cb);
}

static inline int ctr_lte_if_reset(const struct device *dev)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->reset(dev);
}

static inline int ctr_lte_if_wakeup(const struct device *dev)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->wakeup(dev);
}

static inline int ctr_lte_if_enable(const struct device *dev)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->enable(dev);
}

static inline int ctr_lte_if_disable(const struct device *dev)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->disable(dev);
}

static inline int ctr_lte_if_send(const struct device *dev, const char *fmt, va_list ap)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->send(dev, fmt, ap);
}

static inline int ctr_lte_if_send_raw(const struct device *dev, const void *buf, size_t len)
{
	const struct ctr_lte_if_driver_api *api = (const struct ctr_lte_if_driver_api *)dev->api;

	return api->send_raw(dev, buf, len);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_LTE_IF_H_ */
