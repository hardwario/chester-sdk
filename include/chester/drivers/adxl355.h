#ifndef CHESTER_INCLUDE_DRIVERS_ADX355_H_
#define CHESTER_INCLUDE_DRIVERS_ADX355_H_

#include <drivers/i2c.h>

enum adxl355_range {
	ADXL355_RANGE_2G = 1,
	ADXL355_RANGE_4G = 2,
	ADXL355_RANGE_8G = 3,
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
	ADXL355_ODR_LPF_3_906HZ = 10,
};

enum adxl355_hpf {
	ADXL355_HPF_OFF = 0,   /* off */
	ADXL355_HPF_3_862 = 4, /* 3.862 x 10^3 data rate */
	ADXL355_HPF_0_954 = 5, /* 0.954 x 10^3 data rate */
	ADXL355_HPF_0_238 = 6, /* 0.238 x 10^3 data rate */
};

enum adxl355_op_mode {
	ADXL355_OP_MODE_WAKEUP = 0,
	ADXL355_OP_MODE_STANDBY = 1,
};

struct adxl355_data_accelerations {
	double axis_x;
	double axis_y;
	double axis_z;
};

int adxl355_set_range(const struct device *dev, enum adxl355_range val);
int adxl355_set_datarate(const struct device *dev, enum adxl355_odr_lpf val);
int adxl355_set_high_pass_filter(const struct device *dev, enum adxl355_hpf val);
int adxl355_set_op_mode_accel(const struct device *dev, enum adxl355_op_mode mode);
int adxl355_set_op_mode_temp(const struct device *dev, enum adxl355_op_mode mode);
int adxl355_read_data_accel(const struct device *dev,
			    struct adxl355_data_accelerations *accelerations);
int adxl355_read_data_temp(const struct device *dev, float *temperature);

// int adxl355_get_data(struct device *dev);

#endif /* CHESTER_INCLUDE_DRIVERS_ADX355_H_ */