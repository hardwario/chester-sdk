/* Chester includes */
#include <chester/drivers/adxl355.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stdint.h>

#define DT_DRV_COMPAT adi_adxl355

LOG_MODULE_REGISTER(ADXL355, CONFIG_ADXL355_LOG_LEVEL);

#define REG_DEVID_AD  0x00
#define VAL_DEVID_AD  0xAD
#define REG_DEVID_MST 0x01
#define VAL_DEVID_MST 0x1D
#define REG_PARTID    0x02
#define VAL_PARTID    0xED
#define REG_REVID     0x03

#define STATUS_REG	     0x04
#define STATUS_DATA_RDY_MSK  BIT(0)
#define STATUS_FIFO_FULL_MSK BIT(1)
#define STATUS_FIFO_OVR_MSK  BIT(2)
#define STATUS_ACTICITY_MSK  BIT(3)
#define STATUS_NVM_BUSY_MSK  BIT(4)

#define FIFO_ENTRIES_REG 0x05
#define DATA_REG	 0x06
#define DATA_TEMP_REG	 0x06
#define DATA_ACCEL_REG	 0x08
// #define DATA_FIFO_REG 0x11
#define FIFO_SAMPLES_REG 0x29

#define FILTER_REG	       0x28
#define FILTER_ODR_LPF_MSK     GENMASK(3, 0)
#define FILTER_ODR_LPF_MODE(x) (((x)&0xF) << 0)
#define FILTER_HPF_MSK	       GENMASK(6, 4)
#define FILTER_HPF_MODE(x)     (((x)&0x3) << 4)

#define RANGE_REG	    0x2C
#define RANGE_RANGE_MSK	    GENMASK(1, 0)
#define RANGE_RANGE_MODE(x) (((x)&0x3) << 0)

// #define RANGE_INT_POL_MSK               BIT(6)
// #define RANGE_INT_POL_MODE              (((x) & 0x1) << 6)
#define RANGE_I2C_HS_MSK     BIT(7)
#define RANGE_I2C_HS_MODE(x) (((x)&0x1) << 7)

#define POWER_CTL_REG		   0x2D
#define POWER_CTL_STANDBY_MSK	   BIT(0)
#define POWER_CTL_STANDBY_MODE(x)  (((x)&0x1) << 0)
#define POWER_CTL_TEMP_OFF_MSK	   BIT(1)
#define POWER_CTL_TEMP_OFF_MODE(x) (((x)&0x1) << 0)
#define POWER_CTL_DRDY_OFF_MSK	   BIT(2)
#define POWER_CTL_DRDY_OFF_MODE(x) (((x)&0x1) << 0)

#define REG_RESET 0x2F
#define CMD_RESET 0x52

#define RETRIEVES 100

enum adxl355_i2c_speed {
	ADXL355_I2C_SPEED_FAST = 0,
	ADXL355_I2C_SPEED_HIGH = 1
};

struct adxl355_config {
	struct i2c_dt_spec i2c_spec;
	int range;
	int odr_lpf;
	int hpf;
	int i2c_speed;
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
};

static inline const struct adxl355_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct adxl355_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int adxl355_config_to_data(const struct device *dev)
{
	const struct adxl355_config *config = get_config(dev);

	struct adxl355_data *data = get_data(dev);

	data->range = (enum adxl355_range)config->range;
	data->odr_lpf = (enum adxl355_odr_lpf)config->odr_lpf;
	data->hpf = (enum adxl355_hpf)config->hpf;
	data->i2c_speed = (enum adxl355_i2c_speed)config->i2c_speed;

	return 0;
}

static int adxl355_read_byte(const struct device *dev, uint8_t reg, uint8_t *data)
{
	int ret;

	const struct adxl355_config *config = get_config(dev);

	if (!device_is_ready(config->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = i2c_reg_read_byte_dt(&config->i2c_spec, reg, data);
	if (ret) {
		LOG_ERR("Call 'i2c_reg_read_byte_dt' failed: %d", ret);
		return -EINVAL;
	}

	k_sleep(K_MSEC(20));

	return 0;
}

static int adxl355_read(const struct device *dev, uint8_t reg, uint8_t *data, size_t len)
{
	int ret;

	const struct adxl355_config *config = get_config(dev);

	if (!device_is_ready(config->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = i2c_burst_read_dt(&config->i2c_spec, reg, data, len);
	if (ret) {
		LOG_ERR("Call 'i2c_burst_read_dt' failed: %d", ret);
		return -EINVAL;
	}

	return 0;
}

static int adxl355_write_byte(const struct device *dev, uint8_t reg, uint8_t data)
{
	int ret;

	const struct adxl355_config *config = get_config(dev);

	if (!device_is_ready(config->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = i2c_reg_write_byte_dt(&config->i2c_spec, reg, data);
	if (ret) {
		LOG_ERR("Call 'i2c_reg_write_byte_dt' failed: %d", ret);
		return -ENODEV;
	}

	return 0;
}

static int adxl355_read_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t *data)
{
	int ret;
	uint8_t tmp;

	ret = adxl355_read_byte(dev, reg, &tmp);
	if (ret) {
		LOG_ERR("Call `adxl355_read_byte` failed: %d", ret);
		return ret;
	}

	tmp &= mask;
	*data = tmp;

	return 0;
}

static int adxl355_write_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t data)
{
	int ret;
	uint8_t tmp;

	ret = adxl355_read_byte(dev, reg, &tmp);
	if (ret) {
		LOG_ERR("Call `adxl355_read_byte` failed: %d", ret);
		return ret;
	}

	tmp &= ~mask;
	tmp |= data;

	ret = adxl355_write_byte(dev, reg, tmp);
	if (ret) {
		LOG_ERR("Call `adxl355_write_byte` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int adxl355_check_device(const struct device *dev)
{
	int ret;
	uint8_t buf;

	ret = adxl355_read(dev, REG_DEVID_AD, &buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call 'adxl355_read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != VAL_DEVID_AD) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	ret = adxl355_read(dev, REG_DEVID_MST, &buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call 'adxl355_read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != VAL_DEVID_MST) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	ret = adxl355_read(dev, REG_PARTID, &buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call 'adxl355_read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != VAL_PARTID) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	return 0;
};

static int adxl355_get_revision(const struct device *dev, uint8_t *rev)
{
	int ret;

	ret = adxl355_read(dev, REG_REVID, rev, sizeof(rev));
	if (ret) {
		LOG_ERR("Call 'adx355_read' failed: %d", ret);
		return -EINVAL;
	}

	return 0;
}

static int adxl355_reset(const struct device *dev)
{
	int ret;

	ret = adxl355_write_byte(dev, REG_RESET, CMD_RESET);
	if (ret) {
		LOG_ERR("Call 'adxl355_reset' failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(100));

	return 0;
}

static int adxl355_set_range(const struct device *dev, enum adxl355_range val)
{
	int ret;

	ret = adxl355_write_mask(dev, RANGE_REG, RANGE_RANGE_MSK, RANGE_RANGE_MODE(val));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->range = val;

	return 0;
}

static int adxl355_set_odr_lpf(const struct device *dev, enum adxl355_odr_lpf val)
{
	int ret;

	ret = adxl355_write_mask(dev, FILTER_REG, FILTER_ODR_LPF_MSK, FILTER_ODR_LPF_MODE(val));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->odr_lpf = val;

	return 0;
}

static int adxl355_set_hpf(const struct device *dev, enum adxl355_hpf val)
{
	int ret;

	ret = adxl355_write_mask(dev, FILTER_REG, FILTER_HPF_MSK, FILTER_HPF_MODE(val));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->hpf = val;

	return 0;
}

static int adxl355_set_i2c_speed(const struct device *dev, enum adxl355_i2c_speed val)
{
	int ret;

	ret = adxl355_write_mask(dev, RANGE_REG, RANGE_I2C_HS_MSK, RANGE_I2C_HS_MODE(val));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->hpf = val;

	return 0;
}

static int adxl355_set_op_mode_accel(const struct device *dev, enum adxl355_op_mode mode)
{
	int ret;

	ret = adxl355_write_mask(dev, POWER_CTL_REG, POWER_CTL_STANDBY_MSK,
				 POWER_CTL_STANDBY_MODE(mode));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
	}

	get_data(dev)->op_mode = mode;

	k_sleep(K_MSEC(5));

	return 0;
}

static int adxl355_set_op_mode_temp(const struct device *dev, enum adxl355_op_mode mode)
{
	int ret;
	ret = adxl355_write_mask(dev, POWER_CTL_REG, POWER_CTL_TEMP_OFF_MSK,
				 POWER_CTL_TEMP_OFF_MODE(mode));
	if (ret) {
		LOG_ERR("Call 'adxl355_write_mask' failed: %d", ret);
	}

	get_data(dev)->op_mode_temp = mode;

	k_sleep(K_MSEC(20));

	return 0;
}

static int adxl355_set_fifo_samples(const struct device *dev, uint8_t samples)
{
	if (samples > 0 && samples > 96) {
		LOG_ERR("wrong number fifo samples acceptable range is <1-96>");
		return -EINVAL;
	}

	int ret;
	ret = adxl355_write_byte(dev, FIFO_SAMPLES_REG, samples);
	if (ret) {
		LOG_ERR("Call 'adxl355_write_byte' failed: %d", ret);
	}

	get_data(dev)->fifo_samples = samples;
	return 0;
}

static int adxl355_get_fifo_entries(const struct device *dev, uint8_t *entries)
{
	int ret;
	ret = adxl355_read_byte(dev, FIFO_ENTRIES_REG, entries);
	if (ret) {
		LOG_ERR("Call 'adxl355_read_byte' failed: %d", ret);
	}

	return 0;
}

/*
static int adxl355_get_configurations(const struct device *dev)
{
	int ret;
	uint8_t data;

	ret = adxl355_read_byte(dev, REG_DEVID_AD, &data);
	LOG_ERR("%i \t %02x", REG_DEVID_AD, data);

	ret = adxl355_read_byte(dev, REG_DEVID_MST, &data);
	LOG_ERR("%i \t %02x", REG_DEVID_MST, data);

	ret = adxl355_read_byte(dev, STATUS_REG, &data);
	LOG_ERR("%i \t %02x", STATUS_REG, data);

	ret = adxl355_read_byte(dev, FILTER_REG, &data);
	LOG_ERR("%i \t %02x", FILTER_REG, data);

	ret = adxl355_read_byte(dev, RANGE_REG, &data);
	LOG_ERR("%i \t %02x", RANGE_REG, data);

	ret = adxl355_read_byte(dev, POWER_CTL_REG, &data);
	LOG_ERR("%i \t %02x", POWER_CTL_REG, data);

	return 0;
}
*/

static int adxl355_get_status_drdy(const struct device *dev, bool *state)
{
	uint8_t tmp = 0;

	adxl355_read_mask(dev, STATUS_REG, STATUS_DATA_RDY_MSK, &tmp);

	if (tmp == 0x00) {
		*state = false;
		return 0;
	} else if (tmp == 0x01) {
		*state = true;
		return 0;
	} else {
		*state = false;
		return -EINVAL;
	}
}

static int adxl355_get_status_fifo_full(const struct device *dev, bool *state)
{
	uint8_t tmp = 0;

	adxl355_read_mask(dev, STATUS_REG, STATUS_DATA_RDY_MSK, &tmp);

	if (tmp == 0x00) {
		*state = false;
		return 0;
	} else if (tmp == 0x01) {
		*state = true;
		return 0;
	} else {
		*state = false;
		return -EINVAL;
	}
}

static int adxl355_range_to_scale(enum adxl355_range range)
{
	switch (range) {
	case ADXL355_RANGE_2G:
		return 256000;
	case ADXL355_RANGE_4G:
		return 128000;
	case ADXL355_RANGE_8G:
		return 64000;
	default:
		LOG_ERR("Range is not supported");
		return -EINVAL;
	}
}

static int adxl355_normalize_axis(const uint8_t *input, int32_t *result)
{
	int32_t data = ((input[2] >> 4) + (input[1] << 4) + (input[0] << 12));

	int lenght = 1048576;
	int half_lenght = lenght / 2;

	if (data < half_lenght - 1) {
		*result = data;
	} else {
		*result = data - lenght;
	}

	return 0;
}

static int adxl355_normalize_temperature(const uint8_t *input, int16_t *result)
{
	*result = (input[0] << 8) | (input[1]);

	return 0;
}

static int adxl355_convert_acceleration(int *input, enum adxl355_range range,
					struct sensor_value *val)
{
	int ret;

	int scale = adxl355_range_to_scale(range);

	double g = (double)*input;
	g /= scale;

	__ASSERT_NO_MSG(scale != -EINVAL);

	ret = sensor_value_from_double(val, g);
	if (ret) {
		LOG_ERR("Call `sensor_value_from_double` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int adxl355_convert_temperature(int16_t *input, struct sensor_value *val)
{
	int ret;

	float celsius = 25.0f + (*input - 1885) / -9.05f;

	ret = sensor_value_from_double(val, celsius);
	if (ret) {
		LOG_ERR("Call `sensor_value_from_double` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int adxl355_read_data_fifo(const struct device *dev)
{
	int ret;
	uint8_t entries;
	ret = adxl355_get_fifo_entries(dev, &entries);
	if (ret) {
		LOG_ERR("Call 'adxl355_get_fifo_entries' failed: %d", ret);
		return ret;
	}

	if (entries < 1) {
		LOG_DBG("No entries");
		return -ENODATA;
	}

	bool drdy;
	ret = adxl355_get_status_drdy(dev, &drdy);
	if (ret) {
		LOG_ERR("Call 'adxl355_get_status_drdy' failed: %d", ret);
		return ret;
	}

	if (drdy) {
		uint8_t buf[entries][3];

		ret = adxl355_read(dev, DATA_ACCEL_REG, (uint8_t *)buf, sizeof(buf));
		if (ret) {
			LOG_ERR("Call `adxl355_read` failed: %d", ret);
			return ret;
		}

		return 0;
	}

	return -ENODATA;
}

static int adxl355_read_data_accel(const struct device *dev)
{

	/* check operation mode */
	if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY) {
		LOG_ERR("Accelerometer is in standby");
		return -EACCES;
	}

	/* Read data */
	int ret;
	bool drdy;

	for (int i = 0; i < RETRIEVES; i++) {

		ret = adxl355_get_status_drdy(dev, &drdy);

		if (drdy) {
			struct adxl355_data *data = get_data(dev);
			uint8_t buf[9];

			ret = adxl355_read(dev, DATA_ACCEL_REG, (uint8_t *)buf, sizeof(buf));
			if (ret) {
				LOG_ERR("Call `adxl355_read` failed: %d", ret);
				return ret;
			}

			ret = adxl355_normalize_axis(&buf[0], &data->results.axis_x);
			ret = adxl355_normalize_axis(&buf[3], &data->results.axis_y);
			ret = adxl355_normalize_axis(&buf[6], &data->results.axis_z);

			return 0;
		}
		k_sleep(K_MSEC(2));
	}

	return -ENODATA;
}

static int adxl355_read_data_temp(const struct device *dev)
{
	/* check operation mode */
	if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY) {
		LOG_ERR("Measurement is in standby");
		return -EACCES;
	}
	/* check operation mode */
	if (get_data(dev)->op_mode_temp == ADXL355_OP_MODE_STANDBY) {
		LOG_ERR("Thermometer is in standby");
		return -EACCES;
	}

	/* Read data */
	int ret;

	struct adxl355_data *data = get_data(dev);
	uint8_t buf[2];

	ret = adxl355_read(dev, DATA_TEMP_REG, (uint8_t *)buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `adxl355_read` failed: %d", ret);
		return ret;
	}

	ret = adxl355_normalize_temperature(&buf[0], &data->results.temp);
	LOG_ERR("%d", data->results.temp);
	return 0;
}

int adxl355_config_set_range(const struct device *dev, enum adxl355_range val)
{
	int ret;
	ret = adxl355_set_range(dev, val);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_range' failed: %d", ret);
		return ret;
	}

	return 0;
}

int adxl355_config_set_datarate(const struct device *dev, enum adxl355_odr_lpf val)
{
	int ret;
	ret = adxl355_set_odr_lpf(dev, val);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_odr_lpf' failed: %d", ret);
		return ret;
	}

	return 0;
}

int adxl355_config_set_high_pass_filter(const struct device *dev, enum adxl355_hpf val)
{
	int ret;
	ret = adxl355_set_hpf(dev, val);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_hpf' failed: %d", ret);
		return ret;
	}

	return 0;
}

int adxl355_config_set_op_mode_accel(const struct device *dev, enum adxl355_op_mode mode)
{
	return adxl355_set_op_mode_accel(dev, mode);
}

int adxl355_config_set_op_mode_temp(const struct device *dev, enum adxl355_op_mode mode)
{
	return adxl355_set_op_mode_temp(dev, mode);
}

#if defined(CONFIG_SENSOR)

static int adxl355_set_attr_range(const struct device *dev, const struct sensor_value val)
{
	int ret;

	if (val.val1 < 1 && val.val1 > 3) {
		LOG_ERR("attribute is out off range <1-3>");
		return -EINVAL;
	}

	ret = adxl355_set_range(dev, val.val1);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_range' failed: %d", ret);
		return ret;
	}

	return 0;
}

static int adxl355_set_config(const struct device *dev, const struct sensor_value *val)
{
	int ret;

	switch (val->val1) {
	case 1:
		ret = adxl355_set_range(dev, (enum adxl355_range)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_range' failed: %d", ret);
			return ret;
		}
		break;
	case 2:
		ret = adxl355_set_odr_lpf(dev, (enum adxl355_odr_lpf)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_odr_lpf' failed: %d", ret);
			return ret;
		}
		break;
	case 3:
		ret = adxl355_set_hpf(dev, (enum adxl355_hpf)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_hpf' failed: %d", ret);
			return ret;
		}
		break;
	case 4:
		ret = adxl355_set_i2c_speed(dev, (enum adxl355_i2c_speed)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_i2c_speed' failed: %d", ret);
			return ret;
		}
		break;
	case 5:
		ret = adxl355_set_op_mode_accel(dev, (enum adxl355_op_mode)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
			return ret;
		}

		ret = adxl355_set_op_mode_temp(dev, (enum adxl355_op_mode)val->val2);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
			return ret;
		}
		break;
	default:
		LOG_ERR("Attribute is out off range <1-5>");
		return -ENOTSUP;
	}

	return 0;
}

static int adxl355_set_attr(const struct device *dev, enum sensor_channel chan,
			    enum sensor_attribute attr, const struct sensor_value *val)
{
	int ret;

	if (chan != SENSOR_CHAN_ALL) {
		LOG_ERR("Channel is not supported");
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_FULL_SCALE:
		ret = adxl355_set_attr_range(dev, *val);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_attr_range' failed: %d", ret);
			return ret;
		}
		break;
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		ret = adxl355_set_odr_lpf(dev, val->val1);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_attr_range' failed: %d", ret);
			return ret;
		}
		break;
	case SENSOR_ATTR_CONFIGURATION:
		ret = adxl355_set_config(dev, val);
		if (ret) {
			LOG_ERR("Call 'adxl355_set_attr_range' failed: %d", ret);
			return ret;
		}
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int adxl355_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	if (chan != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	int ret;

	/* Wakeup accelerometer*/
	ret = adxl355_set_op_mode_accel(dev, ADXL355_OP_MODE_WAKEUP);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
		return ret;
	}

	/* read accelerations from sensor */
	ret = adxl355_read_data_accel(dev);
	if (ret) {
		LOG_ERR("Call 'adxl355_read_data' failed: %d", ret);
		return ret;
	}

	/* Wakeup thermometer*/
	ret = adxl355_set_op_mode_temp(dev, ADXL355_OP_MODE_WAKEUP);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
		return ret;
	}

	/* read temperature from sensor */
	ret = adxl355_read_data_temp(dev);
	if (ret) {
		LOG_ERR("Call 'adxl355_read_data' failed: %d", ret);
		return ret;
	}

	/* Standby acceleraton */
	ret = adxl355_set_op_mode_accel(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
		return ret;
	}

	/* Standby thermometer */
	ret = adxl355_set_op_mode_temp(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		LOG_ERR("Call 'adxl355_set_op_mode_accel' failed: %d", ret);
		return ret;
	}

	return 0;
}

static int adxl355_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	int ret;

	struct adxl355_data *data = get_data(dev);

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
		ret = adxl355_convert_acceleration(&data->results.axis_x, data->range, val);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		ret = adxl355_convert_acceleration(&data->results.axis_y, data->range, val);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		ret = adxl355_convert_acceleration(&data->results.axis_z, data->range, val);
		break;
	case SENSOR_CHAN_ACCEL_XYZ:
		ret = adxl355_convert_acceleration(&data->results.axis_x, data->range, &val[0]);
		ret = adxl355_convert_acceleration(&data->results.axis_y, data->range, &val[1]);
		ret = adxl355_convert_acceleration(&data->results.axis_z, data->range, &val[2]);
		break;
	case SENSOR_CHAN_DIE_TEMP:
		adxl355_convert_temperature(&data->results.temp, val);
		break;
	case SENSOR_CHAN_ALL:
		ret = adxl355_convert_acceleration(&data->results.axis_x, data->range, &val[0]);
		ret = adxl355_convert_acceleration(&data->results.axis_y, data->range, &val[1]);
		ret = adxl355_convert_acceleration(&data->results.axis_z, data->range, &val[2]);
		adxl355_convert_temperature(&data->results.temp, &val[3]);
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api adxl355_api_funcs = {
	.attr_set = adxl355_set_attr,
	.sample_fetch = adxl355_sample_fetch,
	.channel_get = adxl355_channel_get,
};

#endif

static int adxl355_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = adxl355_check_device(dev);
	if (ret) {
		LOG_ERR("Call 'adxl355_check_device' failed: %d", ret);
		return ret;
	}

	uint8_t rev;
	ret = adxl355_get_revision(dev, &rev);
	if (ret) {
		LOG_ERR("Call 'adxl355_get_revision' failed: %d", ret);
		return -ENODEV;
	}

	LOG_INF("Device found with revision 0x%02x", rev);

	ret = adxl355_reset(dev);
	if (ret) {
		LOG_ERR("Call 'adxl355_reset' failed: %d", ret);
		return ret;
	}

	ret = adxl355_config_to_data(dev);
	if (ret) {
		return ret;
	}

	ret = adxl355_set_range(dev, get_data(dev)->range);
	if (ret) {
		return ret;
	}

	ret = adxl355_set_odr_lpf(dev, get_data(dev)->odr_lpf);
	if (ret) {
		return ret;
	}
	ret = adxl355_set_hpf(dev, get_data(dev)->hpf);
	if (ret) {
		return ret;
	}

	ret = adxl355_set_i2c_speed(dev, get_data(dev)->i2c_speed);
	if (ret) {
		return ret;
	}

	ret = adxl355_set_op_mode_accel(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		return ret;
	}

	ret = adxl355_set_op_mode_temp(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		return ret;
	}

	return 0;
}

#if defined(CONFIG_SENSOR)

#define ADXL355_INIT(n)                                                                            \
	static const struct adxl355_config inst_##n##_config = {                                   \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.range = DT_INST_PROP(n, range),                                                   \
		.odr_lpf = DT_INST_PROP(n, odr_lpf),                                               \
		.hpf = DT_INST_PROP(n, hpf),                                                       \
		.i2c_speed = DT_INST_PROP(n, i2c_speed),                                           \
	};                                                                                         \
	static struct adxl355_data inst_##n##_data = {                                             \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 1, 1),                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, adxl355_init, NULL, &inst_##n##_data, &inst_##n##_config,         \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &adxl355_api_funcs);
#else

#define ADXL355_INIT(n)                                                                            \
	static const struct adxl355_config inst_##n##_config = {                                   \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.range = DT_INST_PROP(n, range),                                                   \
		.odr_lpf = DT_INST_PROP(n, odr_lpf),                                               \
		.hpf = DT_INST_PROP(n, hpf),                                                       \
		.i2c_speed = DT_INST_PROP(n, i2c_speed),                                           \
	};                                                                                         \
	static struct adxl355_data inst_##n##_data = {                                             \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 1, 1),                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, adxl355_init, NULL, &inst_##n##_data, &inst_##n##_config,         \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, NULL);
#endif

DT_INST_FOREACH_STATUS_OKAY(ADXL355_INIT)
