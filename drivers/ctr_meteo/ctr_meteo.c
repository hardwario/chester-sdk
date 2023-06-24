/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/ctr_edge.h>
#include <chester/drivers/ctr_meteo.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_meteo

LOG_MODULE_REGISTER(ctr_meteo, CONFIG_CTR_METEO_LOG_LEVEL);

#define RAINFALL_EDGE_ACTIVE_DURATION	20
#define RAINFALL_EDGE_INACTIVE_DURATION 20
#define RAINFALL_EDGE_COOLDOWN_TIME	100

#define WIND_SPEED_EDGE_ACTIVE_DURATION	  2
#define WIND_SPEED_EDGE_INACTIVE_DURATION 2
#define WIND_SPEED_EDGE_COOLDOWN_TIME	  10

#define R1	    4700
#define DIVIDER(R2) ((float)(R2) / ((float)(R1) + (float)(R2)))

#define WIND_ADC_ANGLE_SIZE 16
static const float wind_adc_angle[WIND_ADC_ANGLE_SIZE] = {
	DIVIDER(33000),	 /* 0°		North	*/
	DIVIDER(6570),	 /* 22.5°		*/
	DIVIDER(8200),	 /* 45°			*/
	DIVIDER(891),	 /* 67.5°		*/
	DIVIDER(1000),	 /* 90°		East	*/
	DIVIDER(688),	 /* 112.5°		*/
	DIVIDER(2200),	 /* 135°		*/
	DIVIDER(1410),	 /* 157.5°		*/
	DIVIDER(3900),	 /* 180°	South	*/
	DIVIDER(3140),	 /* 202.5°		*/
	DIVIDER(16000),	 /* 225°		*/
	DIVIDER(14120),	 /* 247.5°		*/
	DIVIDER(120000), /* 270°	West	*/
	DIVIDER(42120),	 /* 292.5°		*/
	DIVIDER(64900),	 /* 315°		*/
	DIVIDER(21880),	 /* 337.5°		*/
};

#undef R1
#undef DIVIDER

struct ctr_meteo_config {
	const struct device *ctr_x0_dev;
	enum ctr_adc_channel adc_channel_direction;
	enum ctr_adc_channel adc_channel_vdd;
};

struct ctr_meteo_data {
	const struct device *dev;
	int rainfall_counter;
	int wind_speed_counter;
	int64_t wind_speed_timestamp;
};

static inline const struct ctr_meteo_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_meteo_data *get_data(const struct device *dev)
{
	return dev->data;
}

#define RAINFALL_PULSE_MM 0.2794f

static int ctr_meteo_get_rainfall_and_clear_(const struct device *dev, float *rainfall_mm)
{
	*rainfall_mm = get_data(dev)->rainfall_counter * RAINFALL_PULSE_MM;
	get_data(dev)->rainfall_counter = 0;

	return 0;
}

#define PULSES_PER_SECOND_IS_KM_PER_HOUR       2.4
#define PULSES_PER_SECOND_IS_METERS_PER_SECOND (PULSES_PER_SECOND_IS_KM_PER_HOUR / 3.6)

static int ctr_meteo_get_wind_speed_and_clear_(const struct device *dev, float *wind_speed_mps)
{
	int64_t timestamp = k_uptime_get();
	int64_t time_diff = timestamp - get_data(dev)->wind_speed_timestamp;

	if (!time_diff) {
		*wind_speed_mps = 0.f;
		return 0;
	}

	*wind_speed_mps =
		(get_data(dev)->wind_speed_counter * PULSES_PER_SECOND_IS_METERS_PER_SECOND) /
		(time_diff / 1000.f);

	get_data(dev)->wind_speed_counter = 0;
	get_data(dev)->wind_speed_timestamp = timestamp;

	return 0;
}

static float convert_wind_voltage_to_angle(float voltage, float vdd)
{
	float min_diff_value = FLT_MAX;
	float min_diff_angle = 0;

	for (int i = 0; i < WIND_ADC_ANGLE_SIZE; i++) {
		float current_difference = fabs(voltage - wind_adc_angle[i] * vdd);
		if (min_diff_value > current_difference) {
			min_diff_value = current_difference;
			min_diff_angle = 22.5f * i;
		}
	}

	return min_diff_angle;
}

static int ctr_meteo_get_wind_direction_(const struct device *dev, float *direction)
{
	int ret;

	const struct device *ctr_x0_dev = get_config(dev)->ctr_x0_dev;

	if (!device_is_ready(ctr_x0_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	/* PWR enable */
	ret = ctr_x0_set_mode(ctr_x0_dev, CTR_X0_CHANNEL_4, CTR_X0_MODE_PWR_SOURCE);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(5));

	/* Sample sensor */
	uint16_t adc_sample;
	ret = ctr_adc_read(get_config(dev)->adc_channel_direction, &adc_sample);
	float sensor_voltage = NAN;
	if (ret) {
		LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
		return ret;
	}

	sensor_voltage = (float)CTR_ADC_MILLIVOLTS(adc_sample);

	/* Sample VDD */
	ret = ctr_adc_read(get_config(dev)->adc_channel_vdd, &adc_sample);
	float vdd = NAN;
	if (ret) {
		LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
		return ret;
	}

	vdd = (float)CTR_ADC_MILLIVOLTS(adc_sample);

	/* PWR disable */
	ret = ctr_x0_set_mode(ctr_x0_dev, CTR_X0_CHANNEL_4, CTR_X0_MODE_DEFAULT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	if (isnan(sensor_voltage) || isnan(vdd)) {
		return -EINVAL;
	}

	*direction = convert_wind_voltage_to_angle(sensor_voltage, vdd);

	return 0;
}

static void wind_speed_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
				void *user_data)
{
	if (edge_event == CTR_EDGE_EVENT_ACTIVE) {
		const struct device *dev = (struct device *)user_data;
		get_data(dev)->wind_speed_counter++;
	}
}

static void rainfall_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
			      void *user_data)
{
	if (edge_event == CTR_EDGE_EVENT_ACTIVE) {
		const struct device *dev = (struct device *)user_data;
		get_data(dev)->rainfall_counter++;
	}
}

static int init_edge(struct ctr_edge *edge, ctr_edge_cb_t cb, const struct device *dev,
		     const struct device *ctr_x0_dev, enum ctr_x0_channel channel,
		     enum ctr_x0_mode mode, int active_duration, int inactive_duration,
		     int cooldown_time)
{
	int ret;

	ret = ctr_x0_set_mode(ctr_x0_dev, channel, mode);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	const struct gpio_dt_spec *spec;
	ret = ctr_x0_get_spec(ctr_x0_dev, channel, &spec);
	if (ret) {
		LOG_ERR("Call `ctr_x0_get_spec` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_INPUT | GPIO_ACTIVE_LOW);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_init(edge, spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(edge, cb, (void *)dev);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(edge, cooldown_time);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(edge, active_duration);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(edge, inactive_duration);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_watch(edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init_chester_x0(const struct device *dev, const struct device *ctr_x0_dev)
{
	int ret;

	if (!device_is_ready(ctr_x0_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	/* Channel 1: Rainfall input */
	static struct ctr_edge edge_rainfall;
	ret = init_edge(&edge_rainfall, rainfall_callback, dev, ctr_x0_dev, CTR_X0_CHANNEL_1,
			CTR_X0_MODE_NPN_INPUT, RAINFALL_EDGE_ACTIVE_DURATION,
			RAINFALL_EDGE_INACTIVE_DURATION, RAINFALL_EDGE_COOLDOWN_TIME);
	if (ret) {
		LOG_ERR("Call `init_edge` failed: %d", ret);
		return ret;
	}

	/* Channel 2: Wind speed input */
	static struct ctr_edge edge_wind_speed;
	ret = init_edge(&edge_wind_speed, wind_speed_callback, dev, ctr_x0_dev, CTR_X0_CHANNEL_2,
			CTR_X0_MODE_NPN_INPUT, WIND_SPEED_EDGE_ACTIVE_DURATION,
			WIND_SPEED_EDGE_INACTIVE_DURATION, WIND_SPEED_EDGE_COOLDOWN_TIME);
	if (ret) {
		LOG_ERR("Call `init_edge` failed: %d", ret);
		return ret;
	}

	/* Channel 3: Wind direction measurement */
	ret = ctr_x0_set_mode(ctr_x0_dev, CTR_X0_CHANNEL_3, CTR_X0_MODE_DEFAULT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(get_config(dev)->adc_channel_direction);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	/* Channel 4: Wind direction power source */
	ret = ctr_x0_set_mode(ctr_x0_dev, CTR_X0_CHANNEL_4, CTR_X0_MODE_DEFAULT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(get_config(dev)->adc_channel_vdd);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_meteo_init(const struct device *dev)
{
	int ret;

	/* Save initialization timestamp */
	get_data(dev)->wind_speed_timestamp = k_uptime_get();

	ret = init_chester_x0(dev, get_config(dev)->ctr_x0_dev);
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_meteo_driver_api ctr_meteo_driver_api = {
	.get_rainfall_and_clear = ctr_meteo_get_rainfall_and_clear_,
	.get_wind_speed_and_clear = ctr_meteo_get_wind_speed_and_clear_,
	.get_wind_direction = ctr_meteo_get_wind_direction_,
};

#define CTR_METEO_INIT(n)                                                                          \
	static const struct ctr_meteo_config inst_##n##_config = {                                 \
		.ctr_x0_dev = DEVICE_DT_GET(DT_INST_PARENT(n)),                                    \
		.adc_channel_direction = COND_CODE_0(DT_INST_ENUM_IDX(n, slot),                    \
						     (CTR_ADC_CHANNEL_A2), (CTR_ADC_CHANNEL_B2)),  \
		.adc_channel_vdd = COND_CODE_0(DT_INST_ENUM_IDX(n, slot), (CTR_ADC_CHANNEL_A3),    \
					       (CTR_ADC_CHANNEL_B3)),                              \
	};                                                                                         \
	static struct ctr_meteo_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_meteo_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
			      POST_KERNEL, CONFIG_CTR_METEO_INIT_PRIORITY, &ctr_meteo_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_METEO_INIT)

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x0_a), okay) ||                                            \
	DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x0_b), okay)

BUILD_ASSERT(CONFIG_CTR_METEO_INIT_PRIORITY > CONFIG_CTR_X0_INIT_PRIORITY,
	     "CONFIG_CTR_METEO_INIT_PRIORITY must be higher than "
	     "CONFIG_CTR_X0_INIT_PRIORITY");

#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x0_a), okay) ||                                      \
	  DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x0_b), okay) */
