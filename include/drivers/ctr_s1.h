#ifndef CHESTER_INCLUDE_DRIVERS_CTR_S1_H_
#define CHESTER_INCLUDE_DRIVERS_CTR_S1_H_

/* Zephyr includes */
#include <device.h>

/* Standard include */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_s1_buzzer_command {
	CTR_S1_BUZZER_COMMAND_NONE = 0,
	CTR_S1_BUZZER_COMMAND_1X_1_16 = 1,
	CTR_S1_BUZZER_COMMAND_4X_1_16 = 2,
	CTR_S1_BUZZER_COMMAND_1X_1_8 = 3,
	CTR_S1_BUZZER_COMMAND_1X_1_4 = 4,
	CTR_S1_BUZZER_COMMAND_1X_1_2 = 5,
	CTR_S1_BUZZER_COMMAND_2X_1_2 = 6,
	CTR_S1_BUZZER_COMMAND_3X_1_2 = 7,
	CTR_S1_BUZZER_COMMAND_1X_1_1 = 8,
	CTR_S1_BUZZER_COMMAND_1X_2_1 = 9,
};

enum ctr_s1_buzzer_pattern {
	CTR_S1_BUZZER_PATTERN_NONE = 0,
	CTR_S1_BUZZER_PATTERN_1_HZ_1_1 = 1,
	CTR_S1_BUZZER_PATTERN_1_HZ_1_7 = 2,
	CTR_S1_BUZZER_PATTERN_2_HZ_1_1 = 3,
	CTR_S1_BUZZER_PATTERN_2_HZ_1_3 = 4,
	CTR_S1_BUZZER_PATTERN_4_HZ_1_1 = 5,
	CTR_S1_BUZZER_PATTERN_8_HZ_1_1 = 6,
};

enum ctr_s1_led_channel {
	CTR_S1_LED_CHANNEL_R = 0,
	CTR_S1_LED_CHANNEL_G = 1,
	CTR_S1_LED_CHANNEL_B = 2,
};

enum ctr_s1_led_brightness {
	CTR_S1_LED_BRIGHTNESS_NONE = 0x00,
	CTR_S1_LED_BRIGHTNESS_LOW = 0x1f,
	CTR_S1_LED_BRIGHTNESS_MEDIUM = 0x7f,
	CTR_S1_LED_BRIGHTNESS_HIGH = 0xff,
};

enum ctr_s1_led_command {
	CTR_S1_LED_COMMAND_NONE = 0,
	CTR_S1_LED_COMMAND_1X_1_16 = 1,
	CTR_S1_LED_COMMAND_4X_1_16 = 2,
	CTR_S1_LED_COMMAND_1X_1_8 = 3,
	CTR_S1_LED_COMMAND_1X_1_4 = 4,
	CTR_S1_LED_COMMAND_1X_1_2 = 5,
	CTR_S1_LED_COMMAND_2X_1_2 = 6,
	CTR_S1_LED_COMMAND_3X_1_2 = 7,
	CTR_S1_LED_COMMAND_1X_1_1 = 8,
	CTR_S1_LED_COMMAND_1X_2_1 = 9,
};

enum ctr_s1_led_pattern {
	CTR_S1_LED_PATTERN_NONE = 0,
	CTR_S1_LED_PATTERN_1_HZ_1_1 = 1,
	CTR_S1_LED_PATTERN_1_HZ_1_7 = 2,
	CTR_S1_LED_PATTERN_2_HZ_1_1 = 3,
	CTR_S1_LED_PATTERN_2_HZ_1_3 = 4,
	CTR_S1_LED_PATTERN_4_HZ_1_1 = 5,
	CTR_S1_LED_PATTERN_8_HZ_1_1 = 6,
};

enum ctr_s1_pir_sensitivity {
	CTR_S1_PIR_SENSITIVITY_LOW = 0,
	CTR_S1_PIR_SENSITIVITY_MEDIUM = 1,
	CTR_S1_PIR_SENSITIVITY_HIGH = 2,
	CTR_S1_PIR_SENSITIVITY_VERY_HIGH = 3
};

enum ctr_s1_pir_blind_time {
	CTR_S1_PIR_BLIND_TIME_0_5S = 0,
	CTR_S1_PIR_BLIND_TIME_1_0S = 1,
	CTR_S1_PIR_BLIND_TIME_1_5S = 2,
	CTR_S1_PIR_BLIND_TIME_2_0S = 3,
	CTR_S1_PIR_BLIND_TIME_2_5S = 4,
	CTR_S1_PIR_BLIND_TIME_3_0S = 5,
	CTR_S1_PIR_BLIND_TIME_3_5S = 6,
	CTR_S1_PIR_BLIND_TIME_4_0S = 7,
	CTR_S1_PIR_BLIND_TIME_4_5S = 8,
	CTR_S1_PIR_BLIND_TIME_5_0S = 9,
	CTR_S1_PIR_BLIND_TIME_5_5S = 10,
	CTR_S1_PIR_BLIND_TIME_6_0S = 11,
	CTR_S1_PIR_BLIND_TIME_6_5S = 12,
	CTR_S1_PIR_BLIND_TIME_7_0S = 13,
	CTR_S1_PIR_BLIND_TIME_7_5S = 14,
	CTR_S1_PIR_BLIND_TIME_8_0S = 15,
};

enum ctr_s1_event {
	CTR_S1_EVENT_DEVICE_RESET = 15,
	CTR_S1_EVENT_PIR = 12,
	CTR_S1_EVENT_ALTITUDE_CONVERTED = 11,
	CTR_S1_EVENT_PRESSURE_CONVERTED = 10,
	CTR_S1_EVENT_ILLUMINANCE_CONVERTED = 9,
	CTR_S1_EVENT_HUMIDITY_CONVERTED = 8,
	CTR_S1_EVENT_TEMPERATURE_CONVERTED = 7,
	CTR_S1_EVENT_CO2_CALIBRATION_ABC_DONE = 6,
	CTR_S1_EVENT_CO2_CALIBRATION_TARGET_DONE = 5,
	CTR_S1_EVENT_CO2_CONVERTED = 4,
	CTR_S1_EVENT_BUTTON_PRESSED = 3,
	CTR_S1_EVENT_BUTTON_CLICKED = 2,
	CTR_S1_EVENT_BUTTON_HOLD = 1,
	CTR_S1_EVENT_BUTTON_RELEASED = 0,
};

struct ctr_s1_buzzer_param {
	enum ctr_s1_buzzer_command command;
	enum ctr_s1_buzzer_pattern pattern;
};

struct ctr_s1_led_param {
	enum ctr_s1_led_brightness brightness;
	enum ctr_s1_led_command command;
	enum ctr_s1_led_pattern pattern;
};

struct ctr_s1_status {
	bool button_pressed;
};

typedef void (*ctr_s1_user_cb)(const struct device *dev, enum ctr_s1_event event, void *user_data);
typedef int (*ctr_s1_api_set_handler)(const struct device *dev, ctr_s1_user_cb callback,
                                      void *user_data);
typedef int (*ctr_s1_api_enable_interrupts)(const struct device *dev);
typedef int (*ctr_s1_api_apply)(const struct device *dev);
typedef int (*ctr_s1_api_get_status)(const struct device *dev, struct ctr_s1_status *status);
typedef int (*ctr_s1_api_get_serial_number)(const struct device *dev, uint32_t *serial_number);
typedef int (*ctr_s1_api_get_hw_revision)(const struct device *dev, uint16_t *hw_revision);
typedef int (*ctr_s1_api_get_hw_variant)(const struct device *dev, uint32_t *hw_variant);
typedef int (*ctr_s1_api_get_fw_version)(const struct device *dev, uint32_t *fw_version);
typedef int (*ctr_s1_api_get_vendor_name)(const struct device *dev, char *buf, size_t buf_size);
typedef int (*ctr_s1_api_get_product_name)(const struct device *dev, char *buf, size_t buf_size);
typedef int (*ctr_s1_api_set_buzzer)(const struct device *dev,
                                     const struct ctr_s1_buzzer_param *param);
typedef int (*ctr_s1_api_set_led)(const struct device *dev, enum ctr_s1_led_channel channel,
                                  const struct ctr_s1_led_param *param);

typedef int (*ctr_s1_api_temperature_read)(const struct device *dev, float *temperature);
typedef int (*ctr_s1_api_humidity_read)(const struct device *dev, float *humidity);
typedef int (*ctr_s1_api_pir_motion_read)(const struct device *dev, int *pir_count);
typedef int (*ctr_s1_api_pressure_read)(const struct device *dev, float *pressure);
typedef int (*ctr_s1_api_altitude_read)(const struct device *dev, float *altitude);
typedef int (*ctr_s1_api_co2_concentration_read)(const struct device *dev,
                                                 float *co2_concentration);
typedef int (*ctr_s1_api_illuminance_read)(const struct device *dev, float *illuminance);

typedef int (*ctr_s1_api_calibrate_target_co2_concentration)(const struct device *dev,
                                                             float target_co2_concentration);
typedef int (*ctr_s1_api_set_pir_sensitivity)(const struct device *dev,
                                              enum ctr_s1_pir_sensitivity sensitivity);
typedef int (*ctr_s1_api_set_pir_blind_time)(const struct device *dev,
                                             enum ctr_s1_pir_blind_time blind_time);

struct ctr_s1_driver_api {
	ctr_s1_api_set_handler set_handler;
	ctr_s1_api_enable_interrupts enable_interrupts;
	ctr_s1_api_apply apply;
	ctr_s1_api_get_status get_status;
	ctr_s1_api_get_serial_number get_serial_number;
	ctr_s1_api_get_hw_revision get_hw_revision;
	ctr_s1_api_get_hw_variant get_hw_variant;
	ctr_s1_api_get_fw_version get_fw_version;
	ctr_s1_api_get_vendor_name get_vendor_name;
	ctr_s1_api_get_product_name get_product_name;
	ctr_s1_api_set_buzzer set_buzzer;
	ctr_s1_api_set_led set_led;
	ctr_s1_api_temperature_read temperature_read;
	ctr_s1_api_humidity_read humidity_read;
	ctr_s1_api_pir_motion_read pir_motion_read;
	ctr_s1_api_pressure_read pressure_read;
	ctr_s1_api_altitude_read altitude_read;
	ctr_s1_api_illuminance_read illuminance_read;
	ctr_s1_api_co2_concentration_read co2_concentration_read;
	ctr_s1_api_calibrate_target_co2_concentration calibrate_target_co2_concentration;
	ctr_s1_api_set_pir_sensitivity set_pir_sensitivity;
	ctr_s1_api_set_pir_blind_time set_pir_blind_time;
};

static inline int ctr_s1_set_handler(const struct device *dev, ctr_s1_user_cb user_cb,
                                     void *user_data)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->set_handler(dev, user_cb, user_data);
}

static inline int ctr_s1_enable_interrupts(const struct device *dev)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->enable_interrupts(dev);
}

static inline int ctr_s1_apply(const struct device *dev)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->apply(dev);
}

static inline int ctr_s1_get_status(const struct device *dev, struct ctr_s1_status *status)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_status(dev, status);
}

static inline int ctr_s1_get_serial_number(const struct device *dev, uint32_t *serial_number)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_serial_number(dev, serial_number);
}

static inline int ctr_s1_get_hw_revision(const struct device *dev, uint16_t *hw_revision)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_hw_revision(dev, hw_revision);
}

static inline int ctr_s1_get_hw_variant(const struct device *dev, uint32_t *hw_variant)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_hw_variant(dev, hw_variant);
}

static inline int ctr_s1_get_fw_version(const struct device *dev, uint32_t *fw_version)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_fw_version(dev, fw_version);
}

static inline int ctr_s1_get_vendor_name(const struct device *dev, char *buf, size_t buf_size)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_vendor_name(dev, buf, buf_size);
}

static inline int ctr_s1_get_product_name(const struct device *dev, char *buf, size_t buf_size)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->get_product_name(dev, buf, buf_size);
}

static inline int ctr_s1_set_buzzer(const struct device *dev,
                                    const struct ctr_s1_buzzer_param *param)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->set_buzzer(dev, param);
}

static inline int ctr_s1_set_led(const struct device *dev, enum ctr_s1_led_channel channel,
                                 const struct ctr_s1_led_param *param)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->set_led(dev, channel, param);
}

static inline int ctr_s1_temperature_read(const struct device *dev, float *temperature)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->temperature_read(dev, temperature);
}

static inline int ctr_s1_humidity_read(const struct device *dev, float *humidity)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->humidity_read(dev, humidity);
}

static inline int ctr_s1_pir_motion_read(const struct device *dev, int *pir_count)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->pir_motion_read(dev, pir_count);
}

static inline int ctr_s1_co2_concentration_read(const struct device *dev, float *co2)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->co2_concentration_read(dev, co2);
}

static inline int ctr_s1_pressure_read(const struct device *dev, float *pressure)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->pressure_read(dev, pressure);
}

static inline int ctr_s1_altitude_read(const struct device *dev, float *altitude)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->altitude_read(dev, altitude);
}

static inline int ctr_s1_illuminance_read(const struct device *dev, float *illuminance)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->illuminance_read(dev, illuminance);
}

static inline int ctr_s1_calibrate_target_co2_concentration(const struct device *dev,
                                                            float target_co2_concentration)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->calibrate_target_co2_concentration(dev, target_co2_concentration);
}

static inline int ctr_s1_set_pir_sensitivity(const struct device *dev,
                                             enum ctr_s1_pir_sensitivity sensitivity)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->set_pir_sensitivity(dev, sensitivity);
}

static inline int ctr_s1_set_pir_blind_time(const struct device *dev,
                                            enum ctr_s1_pir_blind_time blind_time)
{
	const struct ctr_s1_driver_api *api = (const struct ctr_s1_driver_api *)dev->api;

	return api->set_pir_blind_time(dev, blind_time);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_CTR_S1_H_ */
