#ifndef CHESTER_INCLUDE_DRIVERS_ADX355_H_
#define CHESTER_INCLUDE_DRIVERS_ADX355_H_

#define ADXL355_STATUS_REG	     0x04
#define ADXL355_STATUS_DATA_RDY_MSK  BIT(0)
#define ADXL355_STATUS_FIFO_FULL_MSK BIT(1)
#define ADXL355_STATUS_FIFO_OVR_MSK  BIT(2)
#define ADXL355_STATUS_ACTICITY_MSK  BIT(3)
#define ADXL355_STATUS_NVM_BUSY_MSK  BIT(4)

#define ADXL355_INT_MAP_REG		 0x2A
#define ADXL355_INT_MAP_ACT_EN2_MSK	 BIT(7)
#define ADXL355_INT_MAP_ACT_EN2_MODE(x)	 (((x)&0x1) << 7)
#define ADXL355_INT_MAP_OVR_EN2_MSK	 BIT(6)
#define ADXL355_INT_MAP_OVR_EN2_MODE(x)	 (((x)&0x1) << 6)
#define ADXL355_INT_MAP_FULL_EN2_MSK	 BIT(5)
#define ADXL355_INT_MAP_FULL_EN2_MODE(x) (((x)&0x1) << 5)
#define ADXL355_INT_MAP_DRDY_EN2_MSK	 BIT(4)
#define ADXL355_INT_MAP_DRDY_EN2_MODE(x) (((x)&0x1) << 4)
#define ADXL355_INT_MAP_ACT_EN1_MSK	 BIT(3)
#define ADXL355_INT_MAP_ACT_EN1_MODE(x)	 (((x)&0x1) << 3)
#define ADXL355_INT_MAP_OVR_EN1_MSK	 BIT(2)
#define ADXL355_INT_MAP_OVR_EN1_MODE(x)	 (((x)&0x1) << 2)
#define ADXL355_INT_MAP_FULL_EN1_MSK	 BIT(1)
#define ADXL355_INT_MAP_FULL_EN1_MODE(x) (((x)&0x1) << 1)
#define ADXL355_INT_MAP_DRDY_EN1_MSK	 BIT(0)
#define ADXL355_INT_MAP_DRDY_EN1_MODE(x) (((x)&0x1) << 0)

/* Zephyre includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

/* Standard includes */
#include <math.h>

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
#endif /* CONFIG_ADXL355_TRIGGER */
};

struct adxl355_results_accel {
	int32_t axis_x;
	int32_t axis_y;
	int32_t axis_z;
};

struct adxl355_data {
	struct k_sem lock;
#if defined(CONFIG_ADXL355_FIFO)
	uint8_t results_count;
	struct adxl355_results_accel results_accel[CONFIG_ADXL355_FIFO_SAMPLES / 3];
#else
	struct adxl355_results_accel results_accel;
#endif
	int16_t result_temp;
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
	struct gpio_callback gpio_cb_drdy;
	struct gpio_callback gpio_cb_int1;
	struct gpio_callback gpio_cb_int2;
	sensor_trigger_handler_t handler_drdy;
	sensor_trigger_handler_t handler_int1;
	sensor_trigger_handler_t handler_int2;
	struct sensor_trigger trigger_drdy;
	struct sensor_trigger trigger_int1;
	struct sensor_trigger trigger_int2;
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif /* CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD */
#endif /* CONFIG_ADXL355_TRIGGER */
};

#if defined(CONFIG_ADXL355_TRIGGER)
int adxl355_write_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t data);
int adxl355_get_status(const struct device *dev, uint8_t *status);
int adxl355_init_interrupts(const struct device *dev);
int adxl355_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler);
#endif /* CONFIG_ADXL355_TRIGGER */

#endif /* CHESTER_INCLUDE_DRIVERS_ADX355_H_ */