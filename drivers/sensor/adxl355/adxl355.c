//#include <kernel.h>
//#include <device.h>
#include <sys/byteorder.h>
#include <devicetree.h>
#include <drivers/i2c.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <zephyr.h>

#include "adxl355.h"

#define DT_DRV_COMPAT adi_adxl355

LOG_MODULE_REGISTER(ADXL355, CONFIG_ADXL355_LOG_LEVEL);

#define REG_DEVID_AD 0x00
#define VAL_DEVID_AD 0xAD
#define REG_DEVID_MST 0x01
#define VAL_DEVID_MST 0x1D
#define REG_PARTID 0x02
#define VAL_PARTID 0xED
#define REG_REVID 0x03

//#define REG_FIFO_ENTRIES 0x05
#define TEMP_DATA_REG 0x06
//#define ACCEL_DATA_REG 0x08
//#define ACCEL_DATA_FIFO_REG 0x11
//#define FIFO_SMAPLES_REG 0x29



#define REG_STATUS 0x04
#define POS_STATUS_DATA_RDY 0
#define POS_STATUS_FIFO_FULL 1
#define POS_STATUS_FIFO_OVR 2
#define POS_STATUS_ACTICITY 3
#define POS_STATUS_NVM_BUSY 4

#define FILTER_REG 0x28
#define FILTER_ODR_LPF_MSK              GENMASK(3, 0)
#define FILTER_ODR_LPF_MODE(x)          (((x) & 0xF) << 0)
#define FILTER_HPF_MSK                  GENMASK(6, 4)
#define FILTER_HPF_MODE(x)              (((x) & 0x3) << 4)

#define RANGE_REG 0x2C
#define RANGE_RANGE_MSK                 GENMASK(1, 0)
#define RANGE_RANGE_MODE(x)             (((x) & 0x3) << 0)

//#define RANGE_INT_POL_MSK               BIT(6)
//#define RANGE_INT_POL_MODE              (((x) & 0x1) << 6)
#define RANGE_I2C_HS_MSK                BIT(7)
#define RANGE_I2C_HS_MODE(x)            (((x) & 0x1) << 7)


#define POWER_CTL_REG 0x2D
#define POWER_CTL_STANDBY_MSK           BIT(0)
#define POWER_CTL_STANDBY_MODE(x)       (((x) & 0x1) << 0)
#define POWER_CTL_TEMP_OFF_MSK          BIT(1)
#define POWER_CTL_TEMP_OFF_MODE(x)      (((x) & 0x1) << 1)
//#define POWER_CTL_DRDY_OFF_MSK          BIT(2)
//#define POWER_CTL_DRDY_OFF_MODE(x)      (((x) & 0x1) << 2)

#define REG_RESET 0x2F
#define CMD_RESET 0X52


static inline const struct adxl355_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct adxl355_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int adxl355_config_to_data(const struct  device *dev)
{
    const struct adxl355_config *config = get_config(dev);

    struct adxl355_data *data = get_data(dev);


    data->range = (enum adxl355_range)config->range;
    data->odr_lpf = (enum adxl355_odr_lpf)config->odr_lpf;
    data->hpf = (enum adxl355_hpf)config->hpf;
    data->i2c_speed = (enum adxl355_i2c_speed)config->i2c_speed;
    data->op_mode = (enum adxl355_op_mode)config->op_mode;

    return 0;
}

static int adxl355_read_byte(const struct device *dev, uint8_t reg, 
                    uint8_t *data)
{
    int ret;

    const struct  adxl355_config *config = get_config(dev);

    if(!device_is_ready(config->i2c_spec.bus)){
        LOG_ERR("Bus not ready");
        return -ENODEV;
    }
    
    ret = i2c_reg_read_byte_dt(&config->i2c_spec, reg, data);
    if(ret){
        LOG_ERR("Call 'i2c_reg_read_byte_dt' failed: %d", ret);
        return -EINVAL;
    }

    return 0;
}

static int adxl355_read(const struct device *dev, uint8_t reg, 
                    uint8_t *data, size_t len)
{
    int ret;

    const struct  adxl355_config *config = get_config(dev);

    if(!device_is_ready(config->i2c_spec.bus)){
        LOG_ERR("Bus not ready");
        return -ENODEV;
    }

    ret = i2c_burst_read_dt(&config->i2c_spec, reg, data, len);
    if(ret){
        LOG_ERR("Call 'i2c_burst_read_dt' failed: %d", ret);
        return -EINVAL;
    }

    return 0;
}

static int adxl355_write_byte(const struct device *dev, uint8_t reg, 
                    uint8_t data)
{
    int ret;

    const struct  adxl355_config *config = get_config(dev);

    if(!device_is_ready(config->i2c_spec.bus)){
        LOG_ERR("Bus not ready");
        return -ENODEV;
    }

    
    ret = i2c_reg_write_byte_dt(&config->i2c_spec, reg, data);
    if(ret){
        LOG_ERR("Call 'i2c_reg_write_byte_dt' failed: %d", ret);
        return -ENODEV;
    }

    return 0;
}

int adxl355_write_mask(const struct device *dev, uint8_t reg,
			           uint32_t mask, uint8_t data)
{
	int ret;
	uint8_t tmp;

	ret = adxl355_read_byte(dev, reg, &tmp);
	if (ret) {
		return ret;
    }

	tmp &= ~mask;
	tmp |= data;

	return adxl355_write_byte(dev, reg, tmp);
}

static int adxl355_check_device(const struct  device *dev)
{
    int ret;
    uint8_t buf;

    ret = adxl355_read(dev, REG_DEVID_AD, &buf,sizeof(buf));
    if(ret){
        LOG_ERR("Call 'adxl355_read' failed: %d", ret);
        return -EINVAL;
    }else {
        if(buf != VAL_DEVID_AD) {
            LOG_ERR("Device is not ADXL355");
            return -ENODEV;
        }
    }

    ret = adxl355_read(dev, REG_DEVID_MST, &buf,sizeof(buf));
    if(ret){
        LOG_ERR("Call 'adxl355_read' failed: %d", ret);
        return -EINVAL;
    }else {
        if(buf != VAL_DEVID_MST) {
            LOG_ERR("Device is not ADXL355");
            return -ENODEV;
        }
    }

    ret = adxl355_read(dev, REG_PARTID, &buf,sizeof(buf));
    if(ret){
        LOG_ERR("Call 'adxl355_read' failed: %d", ret);
        return -EINVAL;
    }else {
        if(buf != VAL_PARTID) {
            LOG_ERR("Device is not ADXL355");
            return -ENODEV;
        }
    }

    return 0;
};

static int adxl355_get_revision(const struct device *dev, uint8_t *rev)
{
    int ret;
    

    ret = adxl355_read(dev, REG_REVID, rev, sizeof(rev));
    if(ret){
        LOG_ERR("Call 'adx355_read' failed: %d", ret);
        return -EINVAL;
    }

    return 0;
}

static int adxl355_reset(const struct device *dev)
{
    int ret;
    ret = adxl355_write_byte(dev, REG_RESET, CMD_RESET);
    if(ret){
        return ret;
    }

    k_sleep(K_MSEC(100));

    return 0;
}

static int adxl355_set_range(const struct device *dev, 
                             enum adxl355_range val)
{

    int ret;

    ret = adxl355_write_mask(dev, RANGE_REG,
                                  RANGE_RANGE_MSK, 
                                  RANGE_RANGE_MODE(val));

    if(ret){
        return ret;
    }

    get_data(dev)->range = val;
    
    return 0;
}

static int adxl355_set_odr_lpf(const struct device *dev, 
                               enum adxl355_odr_lpf val)
{

    int ret;

    ret = adxl355_write_mask(dev, FILTER_REG,
                                  FILTER_ODR_LPF_MSK, 
                                  FILTER_ODR_LPF_MODE(val));

    if(ret){
        return ret;
    }

    get_data(dev)->odr_lpf = val;

    return 0;
}

static int adxl355_set_hpf(const struct device *dev, 
                           enum adxl355_hpf val)
{

    int ret;

    ret = adxl355_write_mask(dev, FILTER_REG,
                                  FILTER_HPF_MSK, 
                                  FILTER_HPF_MODE(val));

    if(ret){
        return ret;
    }

    get_data(dev)->hpf = val;

    return 0;
}                            

static int adxl355_set_i2c_speed(const struct device *dev, 
                             enum adxl355_i2c_speed val)
{

    int ret;

    ret = adxl355_write_mask(dev, RANGE_REG,
                                  RANGE_I2C_HS_MSK, 
                                  RANGE_I2C_HS_MODE(val));

    if(ret){
        return ret;
    }

    get_data(dev)->hpf = val;

    return 0;
}   

static int adxl355_set_op_mode(const struct device *dev,
                             enum adxl355_op_mode mode)
{
    int ret;

    ret = adxl355_write_mask(dev, POWER_CTL_REG, POWER_CTL_STANDBY_MSK,
                                  POWER_CTL_STANDBY_MODE(mode) );


    ret = adxl355_write_mask(dev, POWER_CTL_REG, POWER_CTL_TEMP_OFF_MSK,
                                  POWER_CTL_TEMP_OFF_MODE(mode) );
    
    return 0;
}


/* TODO METHODS TO READ DATA FROM FIFO*/


static int adxl355_set_attr_range(const struct device *dev, 
                                  const struct sensor_value val)
{
    int ret;

    if(val.val1 < 1 && val.val1 > 3 )
        return -EINVAL;
    
    ret = adxl355_set_range(dev, val.val1);
    if(ret)
        return ret;

    return 0;
}

static int adxl355_set_config(const struct device *dev,
                              const struct sensor_value *val)
{
    int ret;

    switch (val->val1)
    {
    case 1:
        ret = adxl355_set_range(dev, (enum adxl355_range) val->val2);
        if(ret)
            return ret;
        break;

    case 2:
        ret = adxl355_set_odr_lpf(dev, (enum adxl355_odr_lpf) val->val2);
        if(ret)
            return ret;
        break;
    
    case 3:
        ret = adxl355_set_hpf(dev, (enum adxl355_hpf) val->val2);
        if(ret)
            return ret;
        break;

    case 4:
        ret = adxl355_set_i2c_speed(dev, (enum adxl355_i2c_speed) val->val2);
        if(ret)
            return ret;
        break;
    
    case 5:
        ret = adxl355_set_op_mode(dev, (enum adxl355_op_mode) val->val2);
        if(ret)
            return ret;
        break;
    
    default:
        return -ENOTSUP;
    }

    return 0;
}

static int adxl355_set_attr(const struct device *dev,
                         enum sensor_channel chan,
                         enum sensor_attribute attr,
                         const struct sensor_value *val)
{
    int ret;

    if(chan != SENSOR_CHAN_ALL)
        return -ENOTSUP;
    
    switch (attr)
    {
    case SENSOR_ATTR_FULL_SCALE:
        ret = adxl355_set_attr_range(dev, *val);
        if(ret)
            return ret;
        break;

    case SENSOR_ATTR_SAMPLING_FREQUENCY:
        ret = adxl355_set_odr_lpf(dev, val->val1);
        if(ret)
            return ret;
        break;

    case SENSOR_ATTR_CONFIGURATION:
        ret = adxl355_set_config(dev, val);
        if(ret)
            return ret;
    
    default:
        return -ENOTSUP;
    }

    return 0;
}


static int32_t adxl355_convert_uint20_to_int32_t(uint32_t data)
{
    int lenght = 1048576;
    int half_lenght = lenght / 2;

    if(data < half_lenght)
        return data;

    return data - lenght;
}

static int adxl355_sample_fetch(const struct device *dev,
                                enum sensor_channel chan )
{

    if(chan != SENSOR_CHAN_ALL)
        return -EEXIST;

    int ret;
    struct adxl355_data *data = get_data(dev);
    uint8_t buf[11];

    ret = adxl355_read(dev, TEMP_DATA_REG, (uint8_t *)buf, sizeof(buf));
    if(ret)
        return ret;



    data->data.axis_x = adxl355_convert_uint20_to_int32_t(
        ( ( buf[4] >> 4) +  (buf[3] << 4) + (buf[2] << 12) ));
    data->data.axis_y = adxl355_convert_uint20_to_int32_t(
        ( ( buf[7] >> 4) +  (buf[6] << 4) + (buf[5] << 12) ));
    data->data.axis_z = adxl355_convert_uint20_to_int32_t(
        ( ( buf[10] >> 4) +  (buf[9] << 4) + (buf[8] << 12) ));

    data->data.temp = buf[0] << 8 | buf[1];

    return 0;   
}



static int adxl355_range_to_scale(enum adxl355_range range)
{
    switch (range)
    {
    case ADXL355_RANGE_2G:
        return 256000;
    case ADXL355_RANGE_4G:
        return 128000;
    case ADXL355_RANGE_8G:
        return 64000;
    default:
        return -EINVAL;
    }
}

static void adxl355_accel_convert(struct sensor_value *val,
                                  int accel,
                                  enum adxl355_range range)
{

    
    int scale = adxl355_range_to_scale(range);
    
    double g = (1.0L * accel) / scale;


    __ASSERT_NO_MSG(scale != -EINVAL);

    sensor_value_from_double(val, g);
}

static void adxl355_temp_convert(struct sensor_value *val, int temp)
{
    double celsius = (25 + (temp - 1852) / -9.05f);

    sensor_value_from_double(val, celsius);
}

static int adxl355_channel_get(const struct device *dev,
                              enum sensor_channel chan,
                              struct sensor_value *val)
{
    struct adxl355_data *data = get_data(dev);

    switch (chan)
    {
    case SENSOR_CHAN_ACCEL_X:
        adxl355_accel_convert(val, data->data.axis_x, data->range);
        break;

    case SENSOR_CHAN_ACCEL_Y:
        adxl355_accel_convert(val, data->data.axis_y, data->range);
        break;
    
    case SENSOR_CHAN_ACCEL_Z:
        adxl355_accel_convert(val, data->data.axis_z, data->range);
        break;

    case SENSOR_CHAN_ACCEL_XYZ:
        adxl355_accel_convert(&val[0], data->data.axis_x, data->range);
        adxl355_accel_convert(&val[1], data->data.axis_y, data->range);
        adxl355_accel_convert(&val[2], data->data.axis_z, data->range);
        break;

    case SENSOR_CHAN_DIE_TEMP:
        adxl355_temp_convert(val, data->data.temp);
        break;
    case SENSOR_CHAN_ALL:
        adxl355_accel_convert(&val[0], data->data.axis_x, data->range);
        adxl355_accel_convert(&val[1], data->data.axis_y, data->range);
        adxl355_accel_convert(&val[2], data->data.axis_z, data->range);
        adxl355_temp_convert(&val[3], data->data.temp);
        break;
    default:
        return -ENOTSUP;
    }

    return 0;
}

static const struct sensor_driver_api adxl355_api_funcs = {
    .attr_set = adxl355_set_attr,
    .sample_fetch = adxl355_sample_fetch,
    .channel_get = adxl355_channel_get
};

static int adxl355_init(const struct device *dev)
{
    int ret;

    LOG_INF("System initialization");

    if(!device_is_ready(get_config(dev)->i2c_spec.bus)){
        LOG_ERR("I2C device is not ready");
        return -ENODEV;
    }
    

    ret = adxl355_check_device(dev);
    if(ret){
        LOG_ERR("Call 'adxl355_check_device' failed: %d", ret);
        return ret;
    }
    
    uint8_t rev;

    ret = adxl355_get_revision(dev, &rev);
    if(ret){
        LOG_ERR("Call 'adxl355_get_revision' failed: %d", ret);
        return -ENODEV;
    }
    LOG_INF("Device founf with revision 0X%02X", rev);

    /*
    ret = adxl355_reset(dev);
    if(ret){
        LOG_ERR("Call 'adxl355_reset' failed: %d", ret);
        return ret;
    }
    */

    ret = adxl355_config_to_data(dev);
    if(ret){
        return ret;
    }

    ret = adxl355_set_range(dev, get_data(dev)->range);
    if(ret){
        return ret;
    }

    ret = adxl355_set_odr_lpf(dev, get_data(dev)->odr_lpf);
    if(ret){
        return ret;
    }
    ret = adxl355_set_hpf(dev, get_data(dev)->hpf);
    if(ret){
        return ret;
    }

    ret = adxl355_set_i2c_speed(dev, get_data(dev)->i2c_speed);
    if(ret){
        return ret;
    }

    ret = adxl355_set_op_mode(dev, ADXL355_OP_MODE_STANDBY);
    if(ret){
        return ret;
    }

    return 0;
}

#define ADXL355_INIT(n)                                                                     \
	static const struct adxl355_config inst_##n##_config = {                                \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                                \
        .range = DT_INST_PROP(n, range),                                                     \
        .odr_lpf = DT_INST_PROP(n, odr_lpf),                                                     \
        .hpf = DT_INST_PROP(n, hpf),                                                     \
        .i2c_speed = DT_INST_PROP(n, i2c_speed),                                                     \
        .op_mode = DT_INST_PROP(n, op_mode),                                                     \
	};                                                                                      \
	static struct adxl355_data inst_##n##_data = {                                          \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 1, 1)                              \
	};                                                                                      \
	DEVICE_DT_INST_DEFINE(n, adxl355_init, NULL, &inst_##n##_data, &inst_##n##_config,      \
	                      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &adxl355_api_funcs);       \

DT_INST_FOREACH_STATUS_OKAY(ADXL355_INIT)