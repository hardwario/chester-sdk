/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_CTR_Z_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_Z_H_

/* Zephyr includes */
#include <zephyr/device.h>

/* Standard include */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_z_buzzer_command {
	CTR_Z_BUZZER_COMMAND_NONE = 0,
	CTR_Z_BUZZER_COMMAND_1X_1_16 = 1,
	CTR_Z_BUZZER_COMMAND_4X_1_16 = 2,
	CTR_Z_BUZZER_COMMAND_1X_1_8 = 3,
	CTR_Z_BUZZER_COMMAND_1X_1_4 = 4,
	CTR_Z_BUZZER_COMMAND_1X_1_2 = 5,
	CTR_Z_BUZZER_COMMAND_2X_1_2 = 6,
	CTR_Z_BUZZER_COMMAND_3X_1_2 = 7,
	CTR_Z_BUZZER_COMMAND_1X_1_1 = 8,
	CTR_Z_BUZZER_COMMAND_1X_2_1 = 9,
};

enum ctr_z_buzzer_pattern {
	CTR_Z_BUZZER_PATTERN_OFF = 0,
	CTR_Z_BUZZER_PATTERN_1_HZ_1_1 = 1,
	CTR_Z_BUZZER_PATTERN_1_HZ_1_7 = 2,
	CTR_Z_BUZZER_PATTERN_2_HZ_1_1 = 3,
	CTR_Z_BUZZER_PATTERN_2_HZ_1_3 = 4,
	CTR_Z_BUZZER_PATTERN_4_HZ_1_1 = 5,
	CTR_Z_BUZZER_PATTERN_8_HZ_1_1 = 6,
	CTR_Z_BUZZER_PATTERN_ON = 15,
};

enum ctr_z_led_channel {
	CTR_Z_LED_CHANNEL_0_R = 0,
	CTR_Z_LED_CHANNEL_0_G = 1,
	CTR_Z_LED_CHANNEL_0_B = 2,
	CTR_Z_LED_CHANNEL_1_R = 3,
	CTR_Z_LED_CHANNEL_1_G = 4,
	CTR_Z_LED_CHANNEL_1_B = 5,
	CTR_Z_LED_CHANNEL_2_R = 6,
	CTR_Z_LED_CHANNEL_2_G = 7,
	CTR_Z_LED_CHANNEL_2_B = 8,
	CTR_Z_LED_CHANNEL_3_R = 9,
	CTR_Z_LED_CHANNEL_3_G = 10,
	CTR_Z_LED_CHANNEL_3_B = 11,
	CTR_Z_LED_CHANNEL_4_R = 12,
	CTR_Z_LED_CHANNEL_4_G = 13,
	CTR_Z_LED_CHANNEL_4_B = 14,
};

enum ctr_z_led_brightness {
	CTR_Z_LED_BRIGHTNESS_OFF = 0x00,
	CTR_Z_LED_BRIGHTNESS_LOW = 0x1f,
	CTR_Z_LED_BRIGHTNESS_MEDIUM = 0x7f,
	CTR_Z_LED_BRIGHTNESS_HIGH = 0xff,
};

enum ctr_z_led_command {
	CTR_Z_LED_COMMAND_NONE = 0,
	CTR_Z_LED_COMMAND_1X_1_16 = 1,
	CTR_Z_LED_COMMAND_4X_1_16 = 2,
	CTR_Z_LED_COMMAND_1X_1_8 = 3,
	CTR_Z_LED_COMMAND_1X_1_4 = 4,
	CTR_Z_LED_COMMAND_1X_1_2 = 5,
	CTR_Z_LED_COMMAND_2X_1_2 = 6,
	CTR_Z_LED_COMMAND_3X_1_2 = 7,
	CTR_Z_LED_COMMAND_1X_1_1 = 8,
	CTR_Z_LED_COMMAND_1X_2_1 = 9,
};

enum ctr_z_led_pattern {
	CTR_Z_LED_PATTERN_OFF = 0,
	CTR_Z_LED_PATTERN_1_HZ_1_1 = 1,
	CTR_Z_LED_PATTERN_1_HZ_1_7 = 2,
	CTR_Z_LED_PATTERN_2_HZ_1_1 = 3,
	CTR_Z_LED_PATTERN_2_HZ_1_3 = 4,
	CTR_Z_LED_PATTERN_4_HZ_1_1 = 5,
	CTR_Z_LED_PATTERN_8_HZ_1_1 = 6,
	CTR_Z_LED_PATTERN_ON = 15,
};

enum ctr_z_event {
	CTR_Z_EVENT_DEVICE_RESET = 31,
	CTR_Z_EVENT_DC_CONNECTED = 23,
	CTR_Z_EVENT_DC_DISCONNECTED = 22,
	CTR_Z_EVENT_BUTTON_0_PRESS = 19,
	CTR_Z_EVENT_BUTTON_0_CLICK = 18,
	CTR_Z_EVENT_BUTTON_0_HOLD = 17,
	CTR_Z_EVENT_BUTTON_0_RELEASE = 16,
	CTR_Z_EVENT_BUTTON_1_PRESS = 15,
	CTR_Z_EVENT_BUTTON_1_CLICK = 14,
	CTR_Z_EVENT_BUTTON_1_HOLD = 13,
	CTR_Z_EVENT_BUTTON_1_RELEASE = 12,
	CTR_Z_EVENT_BUTTON_2_PRESS = 11,
	CTR_Z_EVENT_BUTTON_2_CLICK = 10,
	CTR_Z_EVENT_BUTTON_2_HOLD = 9,
	CTR_Z_EVENT_BUTTON_2_RELEASE = 8,
	CTR_Z_EVENT_BUTTON_3_PRESS = 7,
	CTR_Z_EVENT_BUTTON_3_CLICK = 6,
	CTR_Z_EVENT_BUTTON_3_HOLD = 5,
	CTR_Z_EVENT_BUTTON_3_RELEASE = 4,
	CTR_Z_EVENT_BUTTON_4_PRESS = 3,
	CTR_Z_EVENT_BUTTON_4_CLICK = 2,
	CTR_Z_EVENT_BUTTON_4_HOLD = 1,
	CTR_Z_EVENT_BUTTON_4_RELEASE = 0,
};

struct ctr_z_buzzer_param {
	enum ctr_z_buzzer_command command;
	enum ctr_z_buzzer_pattern pattern;
};

struct ctr_z_led_param {
	enum ctr_z_led_brightness brightness;
	enum ctr_z_led_command command;
	enum ctr_z_led_pattern pattern;
};

struct ctr_z_status {
	bool dc_input_connected;
	bool button_0_pressed;
	bool button_1_pressed;
	bool button_2_pressed;
	bool button_3_pressed;
	bool button_4_pressed;
};

typedef void (*ctr_z_user_cb)(const struct device *dev, enum ctr_z_event event, void *user_data);
typedef int (*ctr_z_api_set_handler)(const struct device *dev, ctr_z_user_cb callback,
				     void *user_data);
typedef int (*ctr_z_api_enable_interrupts)(const struct device *dev);
typedef int (*ctr_z_api_apply)(const struct device *dev);
typedef int (*ctr_z_api_get_status)(const struct device *dev, struct ctr_z_status *status);
typedef int (*ctr_z_api_get_vdc_mv)(const struct device *dev, uint16_t *vdc);
typedef int (*ctr_z_api_get_vbat_mv)(const struct device *dev, uint16_t *vbat);
typedef int (*ctr_z_api_get_serial_number)(const struct device *dev, uint32_t *serial_number);
typedef int (*ctr_z_api_get_hw_revision)(const struct device *dev, uint16_t *hw_revision);
typedef int (*ctr_z_api_get_hw_variant)(const struct device *dev, uint32_t *hw_variant);
typedef int (*ctr_z_api_get_fw_version)(const struct device *dev, uint32_t *fw_version);
typedef int (*ctr_z_api_get_vendor_name)(const struct device *dev, char *buf, size_t buf_size);
typedef int (*ctr_z_api_get_product_name)(const struct device *dev, char *buf, size_t buf_size);
typedef int (*ctr_z_api_set_buzzer)(const struct device *dev,
				    const struct ctr_z_buzzer_param *param);
typedef int (*ctr_z_api_set_led)(const struct device *dev, enum ctr_z_led_channel channel,
				 const struct ctr_z_led_param *param);

struct ctr_z_driver_api {
	ctr_z_api_set_handler set_handler;
	ctr_z_api_enable_interrupts enable_interrupts;
	ctr_z_api_apply apply;
	ctr_z_api_get_status get_status;
	ctr_z_api_get_vdc_mv get_vdc_mv;
	ctr_z_api_get_vbat_mv get_vbat_mv;
	ctr_z_api_get_serial_number get_serial_number;
	ctr_z_api_get_hw_revision get_hw_revision;
	ctr_z_api_get_hw_variant get_hw_variant;
	ctr_z_api_get_fw_version get_fw_version;
	ctr_z_api_get_vendor_name get_vendor_name;
	ctr_z_api_get_product_name get_product_name;
	ctr_z_api_set_buzzer set_buzzer;
	ctr_z_api_set_led set_led;
};

static inline int ctr_z_set_handler(const struct device *dev, ctr_z_user_cb user_cb,
				    void *user_data)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->set_handler(dev, user_cb, user_data);
}

static inline int ctr_z_enable_interrupts(const struct device *dev)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->enable_interrupts(dev);
}

static inline int ctr_z_apply(const struct device *dev)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->apply(dev);
}

static inline int ctr_z_get_status(const struct device *dev, struct ctr_z_status *status)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_status(dev, status);
}

static inline int ctr_z_get_vdc_mv(const struct device *dev, uint16_t *vdc)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_vdc_mv(dev, vdc);
}

static inline int ctr_z_get_vbat_mv(const struct device *dev, uint16_t *vbat)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_vbat_mv(dev, vbat);
}

static inline int ctr_z_get_serial_number(const struct device *dev, uint32_t *serial_number)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_serial_number(dev, serial_number);
}

static inline int ctr_z_get_hw_revision(const struct device *dev, uint16_t *hw_revision)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_hw_revision(dev, hw_revision);
}

static inline int ctr_z_get_hw_variant(const struct device *dev, uint32_t *hw_variant)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_hw_variant(dev, hw_variant);
}

static inline int ctr_z_get_fw_version(const struct device *dev, uint32_t *fw_version)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_fw_version(dev, fw_version);
}

static inline int ctr_z_get_vendor_name(const struct device *dev, char *buf, size_t buf_size)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_vendor_name(dev, buf, buf_size);
}

static inline int ctr_z_get_product_name(const struct device *dev, char *buf, size_t buf_size)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->get_product_name(dev, buf, buf_size);
}

static inline int ctr_z_set_buzzer(const struct device *dev, const struct ctr_z_buzzer_param *param)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->set_buzzer(dev, param);
}

static inline int ctr_z_set_led(const struct device *dev, enum ctr_z_led_channel channel,
				const struct ctr_z_led_param *param)
{
	const struct ctr_z_driver_api *api = (const struct ctr_z_driver_api *)dev->api;

	return api->set_led(dev, channel, param);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_Z_H_ */
