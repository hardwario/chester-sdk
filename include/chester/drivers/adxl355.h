#ifndef CHESTER_INCLUDE_DRIVERS_ADX355_H_
#define CHESTER_INCLUDE_DRIVERS_ADX355_H_

/* Zephyre includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

enum adxl355_range {
	ADXL355_RANGE_2G = 1,
	ADXL355_RANGE_4G = 2,
	ADXL355_RANGE_8G = 3
};

enum adxl355_odr_lpf {
	ADXL355_ODR_LPF_4000HZ = 0,
	ADXL355_ODR_LPF_2000HZ = 1,
	ADXL355_ODR_LPF_1000HZ = 2,
	ADXL355_ODR_LPF_500HZ = 3,
	ADXL355_ODR_LPF_250HZ = 4,
	ADXL355_ODR_LPF_125HZ = 5,
	ADXL355_ODR_LPF_62_50HZ = 6,
	ADXL355_ODR_LPF_31_25HZ = 7,
	ADXL355_ODR_LPF_15_625HZ = 8,
	ADXL355_ODR_LPF_7_813HZ = 9,
	ADXL355_ODR_LPF_3_906HZ = 10
};

enum adxl355_hpf {
	ADXL355_HPF_OFF = 0,	// off
	ADXL355_HPF_247 = 1,	// 247     x 10^3 data rate
	ADXL355_HPF_62_084 = 2, // 62.084 x 10^3 data rate
	ADXL355_HPF_15_545 = 3, //  15.545 x 10^3 data rate
	ADXL355_HPF_3_862 = 4,	//   3.862 x 10^3 data rate
	ADXL355_HPF_0_954 = 5,	//   0.954 x 10^3 data rate
	ADXL355_HPF_0_238 = 6,	//   0.238 x 10^3 data rate
};

enum adxl355_i2c_speed {
	ADXL355_I2C_SPEED_FAST = 0,
	ADXL355_I2C_SPEED_HIGH = 1
};

enum adxl355_op_mode {
	ADXL355_OP_MODE_WAKEUP = 0,
	ADXL355_OP_MODE_STANDBY = 1,
};

struct adxl355_config {
	struct i2c_dt_spec i2c_spec;
	int range;
	int odr_lpf;
	int hpf;
	int i2c_speed;
#if defined(CONFIG_ADXL355_TRIGGER)
	struct gpio_dt_spec drdy;
	struct gpio_dt_spec int1;
	struct gpio_dt_spec int2;
#endif
};

struct adxl355_results {
	int32_t axis_x;
	int32_t axis_y;
	int32_t axis_z;
	int16_t temp;
};

struct adxl355_data {
	struct k_sem lock;
	struct adxl355_results results;
	enum adxl355_range range;
	enum adxl355_odr_lpf odr_lpf;
	enum adxl355_hpf hpf;
	enum adxl355_i2c_speed i2c_speed;
	enum adxl355_op_mode op_mode;
	enum adxl355_op_mode op_mode_temp;
	uint8_t fifo_samples;
#if defined(CONFIG_ADXL355_TRIGGER)
	const struct device *dev;
	struct k_mutex trigger_mutex;
#if defined(CONFIG_ADXL355_TRIGGER_PIN_INT1)
	struct gpio_callback gpio_cb_int1;
	sensor_trigger_handler_t handler_int1;
	struct sensor_trigger trigger_int1;
#endif
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif
#endif
};

#if defined(CONFIG_ADXL355_TRIGGER)
int adxl355_write_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t data);
int adxl355_get_status(const struct device *dev, uint8_t *status);
int adxl355_get_status_drdy(const struct device *dev, bool *state);
int adxl355_get_status_fifo_full(const struct device *dev, bool *state);

#endif

#endif /* CHESTER_INCLUDE_DRIVERS_ADX355_H_ */