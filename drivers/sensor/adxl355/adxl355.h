#ifndef ZEPHYR_DRIVERS_SENSOR_ADX355_ADX355_H_
#define ZEPHYR_DRIVERS_SENSOR_ADX355_ADX355_H_

#include <drivers/i2c.h>

enum adxl355_range{
    ADXL355_RANGE_2G = 1,
    ADXL355_RANGE_4G = 2,
    ADXL355_RANGE_8G = 3
};

enum adxl355_odr_lpf{
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

enum adxl355_hpf{           
    ADXL355_HPF_OFF = 0,    // off
    ADXL355_HPF_247 = 1,    // 247     x 10^3 data rate 
    ADXL355_HPF_62_084 = 2, // 62.084 x 10^3 data rate
    ADXL355_HPF_15_545 = 3, //  15.545 x 10^3 data rate
    ADXL355_HPF_3_862 = 4,  //   3.862 x 10^3 data rate
    ADXL355_HPF_0_954 = 5,  //   0.954 x 10^3 data rate
    ADXL355_HPF_0_238 = 6,  //   0.238 x 10^3 data rate
};

enum adxl355_i2c_speed{
    ADXL355_I2C_SPEED_FAST = 0,
    ADXL355_I2C_SPEED_HIGH = 1
};

enum adxl355_op_mode{
    ADXL355_OP_MODE_WAKEUP = 0,
    ADXL355_OP_MODE_STANDBY = 1,
};


struct adxl355_config {
    struct i2c_dt_spec i2c_spec;
    int range;
    int odr_lpf;
    int hpf;
    int i2c_speed;
    int op_mode;
};

struct adxl355_accel_data
{
    int32_t axis_x;
    int32_t axis_y;
    int32_t axis_z;
    int16_t temp;
};

struct adxl355_data {
    struct k_sem lock;
    struct adxl355_accel_data data;
    enum adxl355_range range;
    enum adxl355_odr_lpf odr_lpf;
    enum adxl355_hpf hpf;
    enum adxl355_i2c_speed i2c_speed;
    enum adxl355_op_mode op_mode;
};


#endif /* ZEPHYR_DRIVERS_SENSOR_ADX345_ADX355_H_ */