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

#define DEVID_AD_REG  0x00
#define DEVID_AD_VAL  0xAD
#define DEVID_MST_REG 0x01
#define DEVID_MST_VAL 0x1D
#define PARTID_REG    0x02
#define PARTID_VAL    0xED
#define REVID_REG     0x03

#define FIFO_ENTRIES_REG 0x05
#define DATA_REG	 0x06
#define DATA_TEMP_REG	 0x06
#define DATA_ACCEL_REG	 0x08
#define DATA_FIFO_REG	 0x11

#define FILTER_REG	       0x28
#define FILTER_ODR_LPF_MSK     GENMASK(3, 0)
#define FILTER_ODR_LPF_MODE(x) (((x)&0xF) << 0)
#define FILTER_HPF_MSK	       GENMASK(6, 4)
#define FILTER_HPF_MODE(x)     (((x)&0x1) << 4)

#define FIFO_SAMPLES_REG 0x29

#define RANGE_REG	      0x2C
#define RANGE_RANGE_MSK	      GENMASK(1, 0)
#define RANGE_RANGE_MODE(x)   (((x)&0x3) << 0)
#define RANGE_INT_POL_MSK     BIT(6)
#define RANGE_INT_POL_MODE(x) (((x)&0x1) << 6)
#define RANGE_I2C_HS_MSK      BIT(7)
#define RANGE_I2C_HS_MODE(x)  (((x)&0x1) << 7)

#define POWER_CTL_REG		   0x2D
#define POWER_CTL_STANDBY_MSK	   BIT(0)
#define POWER_CTL_STANDBY_MODE(x)  (((x)&0x1) << 0)
#define POWER_CTL_TEMP_OFF_MSK	   BIT(1)
#define POWER_CTL_TEMP_OFF_MODE(x) (((x)&0x1) << 1)
#define POWER_CTL_DRDY_OFF_MSK	   BIT(2)
#define POWER_CTL_DRDY_OFF_MODE(x) (((x)&0x1) << 2)

#define RESET_REG 0x2F
#define RESET_CMD 0x52

#define RETRIEVES 3

static inline const struct adxl355_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct adxl355_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int config_to_data(const struct device *dev)
{
	const struct adxl355_config *config = get_config(dev);

	struct adxl355_data *data = get_data(dev);

	data->range = (enum adxl355_range)config->range;
	data->odr_lpf = (enum adxl355_odr_lpf)config->odr_lpf;
	data->hpf = (enum adxl355_hpf)config->hpf;
	data->i2c_speed = (enum adxl355_i2c_speed)config->i2c_speed;

	return 0;
}

static int read_byte(const struct device *dev, uint8_t reg, uint8_t *data)
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

static int read(const struct device *dev, uint8_t reg, uint8_t *data, size_t len)
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

static int write_byte(const struct device *dev, uint8_t reg, uint8_t data)
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

static int read_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t *data)
{
	int ret;
	uint8_t tmp;

	ret = read_byte(dev, reg, &tmp);
	if (ret) {
		LOG_ERR("Call `read_byte` failed: %d", ret);
		return ret;
	}

	tmp &= mask;
	*data = tmp;

	return 0;
}

static int write_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t data)
{
	int ret;
	uint8_t tmp;

	ret = read_byte(dev, reg, &tmp);
	if (ret) {
		LOG_ERR("Call `read_byte` failed: %d", ret);
		return ret;
	}

	tmp &= ~mask;
	tmp |= data;

	ret = write_byte(dev, reg, tmp);
	if (ret) {
		LOG_ERR("Call `write_byte` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(20));

	return 0;
}

static int check_device(const struct device *dev)
{
	int ret;
	uint8_t buf;

	ret = read_byte(dev, DEVID_AD_REG, &buf);
	if (ret) {
		LOG_ERR("Call 'read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != DEVID_AD_VAL) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	ret = read_byte(dev, DEVID_MST_REG, &buf);
	if (ret) {
		LOG_ERR("Call 'read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != DEVID_MST_VAL) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	ret = read_byte(dev, PARTID_REG, &buf);
	if (ret) {
		LOG_ERR("Call 'read' failed: %d", ret);
		return -EINVAL;
	} else if (buf != PARTID_VAL) {
		LOG_ERR("Device is not ADXL355");
		return -ENODEV;
	}

	return 0;
};

static int get_revision(const struct device *dev, uint8_t *rev)
{
	int ret;

	ret = read_byte(dev, REVID_REG, rev);
	if (ret) {
		LOG_ERR("Call 'read' failed: %d", ret);
		return -EINVAL;
	}

	return 0;
}

static int reset(const struct device *dev)
{
	int ret;

	ret = write_byte(dev, RESET_REG, RESET_CMD);
	if (ret) {
		LOG_ERR("Call 'write_byte' failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(100));

	return 0;
}

static int set_range(const struct device *dev, enum adxl355_range val)
{
	int ret;

	ret = write_mask(dev, RANGE_REG, RANGE_RANGE_MSK, RANGE_RANGE_MODE(val));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->range = val;

	return 0;
}

static int set_odr_lpf(const struct device *dev, enum adxl355_odr_lpf val)
{
	int ret;

	ret = write_mask(dev, FILTER_REG, FILTER_ODR_LPF_MSK, FILTER_ODR_LPF_MODE(val));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->odr_lpf = val;

	return 0;
}

static int set_hpf(const struct device *dev, enum adxl355_hpf val)
{
	int ret;

	ret = write_mask(dev, FILTER_REG, FILTER_HPF_MSK, FILTER_HPF_MODE(val));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->hpf = val;

	return 0;
}

static int set_i2c_speed(const struct device *dev, enum adxl355_i2c_speed val)
{
	int ret;

	ret = write_mask(dev, RANGE_REG, RANGE_I2C_HS_MSK, RANGE_I2C_HS_MODE(val));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	k_sleep((K_MSEC(10)));

	get_data(dev)->i2c_speed = val;

	return 0;
}

static int set_int_polarity(const struct device *dev, enum adxl355_int_pol val)
{
	int ret;

	ret = write_mask(dev, RANGE_REG, RANGE_INT_POL_MSK, RANGE_INT_POL_MODE(val));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	return 0;
}

static int set_op_mode_accel(const struct device *dev, enum adxl355_op_mode mode)
{

	if (get_data(dev)->op_mode != mode) {

		int ret;
		ret = write_mask(dev, POWER_CTL_REG, POWER_CTL_STANDBY_MSK,
				 POWER_CTL_STANDBY_MODE(mode));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
			return ret;
		}

		get_data(dev)->op_mode = mode;
	}

	return 0;
}

static int set_op_mode_temp(const struct device *dev, enum adxl355_op_mode mode)
{
	int ret;
	ret = write_mask(dev, POWER_CTL_REG, POWER_CTL_TEMP_OFF_MSK, POWER_CTL_TEMP_OFF_MODE(mode));
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	get_data(dev)->op_mode_temp = mode;

	return 0;
}

#if defined(CONFIG_ADXL355_FIFO)

static int set_fifo_samples(const struct device *dev, uint8_t samples)
{
	if (samples < 0 && samples > 96) {
		LOG_ERR("wrong number fifo samples acceptable range is <1-96>");
		return -EINVAL;
	}

	int ret;
	ret = write_byte(dev, FIFO_SAMPLES_REG, samples);
	if (ret) {
		LOG_ERR("Call 'write_byte' failed: %d", ret);
	}

	get_data(dev)->fifo_samples = samples;

	return 0;
}

static int get_fifo_entries(const struct device *dev, uint8_t *entries)
{
	int ret;

	ret = read_byte(dev, FIFO_ENTRIES_REG, entries);
	if (ret) {
		LOG_ERR("Call 'read_byte' failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* CONFIG_ADXL355_FIFO */

static int get_configurations(const struct device *dev)
{
	int ret;
	uint8_t data;

	ret = read_byte(dev, DEVID_AD_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", DEVID_AD_REG, data);

	ret = read_byte(dev, DEVID_MST_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", DEVID_MST_REG, data);

	ret = read_byte(dev, ADXL355_STATUS_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", ADXL355_STATUS_REG, data);

	ret = read_byte(dev, FILTER_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", FILTER_REG, data);

	ret = read_byte(dev, FIFO_SAMPLES_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", FIFO_SAMPLES_REG, data);

	ret = read_byte(dev, ADXL355_INT_MAP_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", ADXL355_INT_MAP_REG, data);

	ret = read_byte(dev, RANGE_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", RANGE_REG, data);

	ret = read_byte(dev, POWER_CTL_REG, &data);
	LOG_DBG("reg: 0x%02x = 0x%02x", POWER_CTL_REG, data);

	return 0;
}

int adxl355_write_mask(const struct device *dev, uint8_t reg, uint32_t mask, uint8_t data)
{
	int ret;

	ret = write_mask(dev, reg, mask, data);
	if (ret) {
		LOG_ERR("Call 'write_mask' failed: %d", ret);
		return ret;
	}

	return 0;
}

int adxl355_get_status(const struct device *dev, uint8_t *status)
{
	int ret;

	ret = read_byte(dev, ADXL355_STATUS_REG, status);
	if (ret) {
		LOG_ERR("Call 'read_byte' failed: %d", ret);
		return ret;
	}

	return 0;
}

int adxl355_get_status_drdy(const struct device *dev, bool *state)
{
	uint8_t tmp = 0;

	read_mask(dev, ADXL355_STATUS_REG, ADXL355_STATUS_DATA_RDY_MSK, &tmp);

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

static int range_to_scale(enum adxl355_range range)
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

static int normalize_axis(const uint8_t *input, int32_t *result)
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

static int normalize_temperature(const uint8_t *input, int16_t *result)
{
	*result = (input[0] << 8) | (input[1]);

	return 0;
}

static int convert_acceleration(int *input, enum adxl355_range range, double *acceleration)
{
	int scale = range_to_scale(range);

	double g = (double)*input;
	g /= scale;

	__ASSERT_NO_MSG(scale != -EINVAL);

	*acceleration = g;

	return 0;
}

static int convert_temperature(int16_t *input, float *temperature)
{
	*temperature = 25.0f + (*input - 1885) / -9.05f;

	return 0;
}

static int convert_acceleration_value(int *input, enum adxl355_range range,
				      struct sensor_value *val)
{
	int ret;

	double g;
	ret = convert_acceleration(input, range, &g);
	if (ret) {
		LOG_ERR("Call 'convert_acceleration' failed: %d", ret);
		return ret;
	}

	__ASSERT_NO_MSG(scale != -EINVAL);

	ret = sensor_value_from_double(val, g);
	if (ret) {
		LOG_ERR("Call `sensor_value_from_double` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int convert_temperature_value(int16_t *input, struct sensor_value *val)
{
	int ret;

	float temperature;

	ret = convert_temperature(input, &temperature);
	if (ret) {
		LOG_ERR("Call 'convert_temperature' failed: %d", ret);
		return ret;
	}

	ret = sensor_value_from_double(val, temperature);
	if (ret) {
		LOG_ERR("Call `sensor_value_from_double` failed: %d", ret);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_ADXL355_FIFO)

static int read_data_accel(const struct device *dev)
{
	/* check operation mode */
	if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY) {
		LOG_ERR("Accelerometer is in standby");
		return -EACCES;
	}

	struct adxl355_data *data = get_data(dev);

	int ret;

	uint8_t entries;
	ret = get_fifo_entries(dev, &entries);
	if (ret) {
		LOG_ERR("Call 'get_fifo_entries' failed: %d", ret);
		return ret;
	}

	LOG_DBG("entries %d", entries);

	if (entries > 0) {

		uint8_t buf[ADXL355_FIFO_SAMPLES_MAX][3];

		ret = read(dev, DATA_FIFO_REG, (uint8_t *)buf, entries * 3);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		int result_id = 0;
		for (int i = 0; i < entries;) {
			ret = normalize_axis(&buf[i++][0], &data->results_accel[result_id].axis_x);
			ret = normalize_axis(&buf[i++][0], &data->results_accel[result_id].axis_y);
			ret = normalize_axis(&buf[i++][0], &data->results_accel[result_id].axis_z);
			result_id++;
		}

		get_data(dev)->results_count = result_id;

		return 0;
	}

	return -ENODATA;
}

int adxl355_clear_interrupts(const struct device *dev)
{
	int ret;
	bool drdy;

	ret = adxl355_get_status_drdy(dev, &drdy);
	if (ret) {
		LOG_ERR("Call 'adxl355_get_status_drdy' failed: %d", ret);
		return ret;
	}

	uint8_t entries;
	ret = get_fifo_entries(dev, &entries);
	if (ret) {
		LOG_ERR("Call 'get_fifo_entries' failed: %d", ret);
		return ret;
	}

	uint8_t buf[entries][3];

	ret = read(dev, DATA_FIFO_REG, (uint8_t *)buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	return 0;
}

#else /* CONFIG_ADXL355_FIFO */

static int read_data_accel(const struct device *dev)
{

	/* check operation mode */
	if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY) {
		LOG_ERR("Accelerometer is in standby");
		return -EACCES;
	}

	/* Read data */
	int ret;

	for (int i = 0; i < RETRIEVES; i++) {

#if !defined(CONFIG_ADXL355_TRIGGER) /* Check data ready by status register*/
		bool drdy;
		ret = adxl355_get_status_drdy(dev, &drdy);
		if (drdy == false) {
			k_sleep(K_MSEC(2));
			continue;
		}
#endif
		struct adxl355_data *data = get_data(dev);
		uint8_t buf[9];

		ret = read(dev, DATA_ACCEL_REG, (uint8_t *)buf, sizeof(buf));
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		ret = normalize_axis(&buf[0], &data->results_accel.axis_x);
		if (ret) {
			LOG_ERR("Call `normalize_axis` failed: %d", ret);
			return ret;
		}
		ret = normalize_axis(&buf[3], &data->results_accel.axis_y);
		if (ret) {
			LOG_ERR("Call `normalize_axis` failed: %d", ret);
			return ret;
		}
		ret = normalize_axis(&buf[6], &data->results_accel.axis_z);
		if (ret) {
			LOG_ERR("Call `normalize_axis` failed: %d", ret);
			return ret;
		}

		return 0;
	}

	return -ENODATA;
}

int adxl355_clear_interrupts(const struct device *dev)
{
	int ret;

	bool drdy;

	ret = adxl355_get_status_drdy(dev, &drdy);
	if (ret) {
		LOG_ERR("Call 'adxl355_get_status_drdy' failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* CONFIG_ADXL355_FIFO */

static int read_data_temp(const struct device *dev)
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

	ret = read(dev, DATA_TEMP_REG, (uint8_t *)buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	ret = normalize_temperature(&buf[0], &data->result_temp);
	if (ret) {
		LOG_ERR("Call `normalize_temperature` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int set_attr_range(const struct device *dev, const struct sensor_value val)
{
	int ret;

	if (val.val1 < 1 && val.val1 > 3) {
		LOG_ERR("attribute is out off range <1-3>");
		return -EINVAL;
	}

	ret = set_range(dev, (enum adxl355_range)val.val1);
	if (ret) {
		LOG_ERR("Call 'set_range' failed: %d", ret);
		return ret;
	}

	return 0;
}

static int set_config(const struct device *dev, const struct sensor_value *val)
{
	int ret;

	switch (val->val1) {
	case 1: /* Configure range */
		ret = set_range(dev, (enum adxl355_range)val->val2);
		if (ret) {
			LOG_ERR("Call 'set_range' failed: %d", ret);
			return ret;
		}
		break;
	case 2: /* Configure data rate (low pass filter)*/
		ret = set_odr_lpf(dev, (enum adxl355_odr_lpf)val->val2);
		if (ret) {
			LOG_ERR("Call 'set_odr_lpf' failed: %d", ret);
			return ret;
		}
		break;
	case 3: /* configure high pass filter */
		ret = set_hpf(dev, (enum adxl355_hpf)val->val2);
		if (ret) {
			LOG_ERR("Call 'set_hpf' failed: %d", ret);
			return ret;
		}
		break;
	case 4: /* Wakeup or stand by measuring */
		ret = set_op_mode_accel(dev, (enum adxl355_op_mode)val->val2);
		if (ret) {
			LOG_ERR("Call 'set_op_mode_accel' failed: %d", ret);
			return ret;
		}

		ret = set_op_mode_temp(dev, (enum adxl355_op_mode)val->val2);
		if (ret) {
			LOG_ERR("Call 'set_op_mode_temp' failed: %d", ret);
			return ret;
		}
		break;

	default:
		LOG_ERR("Attribute is out off range <1-5>");
		return -ENOTSUP;
	}

	return 0;
}

static int set_attr(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr,
		    const struct sensor_value *val)
{
	int ret;

	if (chan != SENSOR_CHAN_ALL) {
		LOG_ERR("Channel is not supported");
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_FULL_SCALE:
		ret = set_attr_range(dev, *val);
		if (ret) {
			LOG_ERR("Call 'set_attr_range' failed: %d", ret);
			return ret;
		}
		break;
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		ret = set_odr_lpf(dev, val->val1);
		if (ret) {
			LOG_ERR("Call 'set_odr_lpf' failed: %d", ret);
			return ret;
		}
		break;
	case SENSOR_ATTR_CONFIGURATION:
		ret = set_config(dev, val);
		if (ret) {
			LOG_ERR("Call 'set_config' failed: %d", ret);
			return ret;
		}
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

#if defined(CONFIG_ADXL355_FIFO)

static int get_attr(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr,
		    struct sensor_value *val)
{
	if (chan != SENSOR_CHAN_ACCEL_XYZ) {
		LOG_ERR("Channel is not supported");
		return -ENOTSUP;
	}

	switch (attr) {
		{
		case SENSOR_ATTR_COMMON_COUNT:
			val->val1 = get_data(dev)->results_count;
			break;
		default:
			return -ENOTSUP;
		}
	}

	return 0;
}

#endif

static int sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_ACCEL_XYZ &&
	    chan != SENSOR_CHAN_DIE_TEMP) {

		LOG_ERR("Chanel is not supprted");
		return -ENOTSUP;
	}

	int ret;

	if (chan == SENSOR_CHAN_ACCEL_XYZ || chan == SENSOR_CHAN_ALL) {
		if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY) {
			LOG_ERR("device is in standby mode");
			return -EIO;
		}

		/* read accelerations from sensor */
		ret = read_data_accel(dev);
		if (ret) {
			LOG_ERR("Call 'read_data_accel' failed: %d", ret);
			return ret;
		}
	}

	if (chan == SENSOR_CHAN_DIE_TEMP || chan == SENSOR_CHAN_ALL) {

		if (get_data(dev)->op_mode == ADXL355_OP_MODE_STANDBY ||
		    get_data(dev)->op_mode_temp == ADXL355_OP_MODE_STANDBY) {
			LOG_ERR("device is in standby mode");
			return -EIO;
		}

		/* read temperature from sensor */
		ret = read_data_temp(dev);
		if (ret) {
			LOG_ERR("Call 'read_data' failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
	int ret, val_id = 0;

#if defined(CONFIG_ADXL355_FIFO)
	int i = 0;
#endif

	struct adxl355_data *data = get_data(dev);

	switch (chan) {
	case SENSOR_CHAN_DIE_TEMP:
		ret = convert_temperature_value(&data->result_temp, val);
		break;

#if defined(CONFIG_ADXL355_FIFO)
	case SENSOR_CHAN_ACCEL_XYZ:
		while (i < data->results_count * 3) {
			ret = convert_acceleration_value(&data->results_accel[val_id].axis_x,
							 data->range, &val[i++]);
			ret = convert_acceleration_value(&data->results_accel[val_id].axis_y,
							 data->range, &val[i++]);
			ret = convert_acceleration_value(&data->results_accel[val_id].axis_z,
							 data->range, &val[i++]);

			val_id++;
		}
		break;
#else /* CONFIG_ADXL355_FIFO */
	case SENSOR_CHAN_ACCEL_XYZ:
		ret = convert_acceleration_value(&data->results_accel.axis_x, data->range,
						 &val[val_id++]);
		ret = convert_acceleration_value(&data->results_accel.axis_y, data->range,
						 &val[val_id++]);
		ret = convert_acceleration_value(&data->results_accel.axis_z, data->range,
						 &val[val_id++]);
		break;
	case SENSOR_CHAN_ALL:
		ret = convert_acceleration_value(&data->results_accel.axis_x, data->range,
						 &val[val_id++]);
		ret = convert_acceleration_value(&data->results_accel.axis_y, data->range,
						 &val[val_id++]);
		ret = convert_acceleration_value(&data->results_accel.axis_z, data->range,
						 &val[val_id++]);
		ret = convert_temperature_value(&data->result_temp, &val[val_id]);
		break;

#endif /* CONFIG_ADXL355_FIFO */
	default:
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api adxl355_api_funcs = {
	.attr_set = set_attr,
	.sample_fetch = sample_fetch,
	.channel_get = channel_get,
#if defined(CONFIG_ADXL355_TRIGGER)
	.trigger_set = adxl355_trigger_set,
#if defined(CONFIG_ADXL355_FIFO)
	.attr_get = get_attr,
#endif /* CONFIG_ADXL355_FIFO */
#endif /* CONFIG_ADXL355_TRIGGER */
};

static int init(const struct device *dev)
{
	int ret;

	LOG_INF("Driver initialization");
	k_sleep(K_MSEC(100));

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = check_device(dev);
	if (ret) {
		LOG_ERR("Call 'check_device' failed: %d", ret);
		return ret;
	}

	uint8_t rev;
	ret = get_revision(dev, &rev);
	if (ret) {
		LOG_ERR("Call 'get_revision' failed: %d", ret);
		return -ENODEV;
	} else {
		LOG_INF("Device found with revision 0x%02x", rev);
	}

	ret = reset(dev);
	if (ret) {
		LOG_ERR("Call 'reset' failed %d", ret);
		return ret;
	}

	ret = config_to_data(dev);
	if (ret) {
		LOG_ERR("Call 'config_to_data' failed %d", ret);
		return ret;
	}

	ret = set_i2c_speed(dev, get_data(dev)->i2c_speed);
	if (ret) {
		LOG_ERR("Call 'set_i2c_speed' failed %d", ret);
		return ret;
	}

	ret = set_range(dev, get_data(dev)->range);
	if (ret) {
		LOG_ERR("Call 'set_range' failed %d", ret);
		return ret;
	}

	ret = set_odr_lpf(dev, get_data(dev)->odr_lpf);
	if (ret) {
		LOG_ERR("Call 'set_odr_lpf' failed %d", ret);
		return ret;
	}

	ret = set_hpf(dev, get_data(dev)->hpf);
	if (ret) {
		LOG_ERR("Call 'set_hpf' failed %d", ret);
		return ret;
	}

#if defined(CONFIG_ADXL355_TRIGGER)
	if (get_config(dev)->drdy.port || get_config(dev)->int1.port ||
	    get_config(dev)->int2.port) {

		ret = adxl355_init_interrupts(dev);
		if (ret) {
			LOG_ERR("Call 'adxl355_init_interrupts' failed: %d", ret);
			return -EIO;
		}

		ret = set_int_polarity(dev, ADXL355_INT_POL_HIGH);
		if (ret) {
			LOG_ERR("Call 'set_int_pol' failed: %d", ret);
			return ret;
		}
	}
#endif /* CONFIG_ADXL355_TRIGGER */

#if defined(CONFIG_ADXL355_FIFO)
	uint8_t samples = 96;
#if defined(CONFIG_ADXL355_FIFO_SAMPLES)
	samples = CONFIG_ADXL355_FIFO_SAMPLES;
#endif /* CONFIG_ADXL355_FIFO_SAMPLES */

	ret = set_fifo_samples(dev, samples);
	if (ret) {
		LOG_ERR("Call 'set_fifo_samples' failed %d", ret);
		return ret;
	}

#endif /* CONFIG_ADXL355_FIFO */

	ret = set_op_mode_temp(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		LOG_ERR("Call 'set_op_mode_temp' failed %d", ret);
		return ret;
	}

	ret = set_op_mode_accel(dev, ADXL355_OP_MODE_STANDBY);
	if (ret) {
		LOG_ERR("Call 'set_op_mode_accel' failed %d", ret);
		return ret;
	}

	ret = get_configurations(dev);

	return 0;
}

#define ADXL355_INIT(n)                                                                            \
	static const struct adxl355_config inst_##n##_config = {                                   \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.i2c_speed = DT_INST_PROP(n, i2c_speed),                                           \
		.range = DT_INST_PROP(n, range),                                                   \
		.odr_lpf = DT_INST_PROP(n, odr_lpf),                                               \
		.hpf = DT_INST_PROP(n, hpf),                                                       \
		IF_ENABLED(CONFIG_ADXL355_TRIGGER,                                                 \
			   (.drdy = GPIO_DT_SPEC_INST_GET_OR(n, drdy_gpios, {0}),                  \
			    .int1 = GPIO_DT_SPEC_INST_GET_OR(n, int1_gpios, {0}),                  \
			    .int2 = GPIO_DT_SPEC_INST_GET_OR(n, int2_gpios, {0}), ))};             \
	static struct adxl355_data inst_##n##_data = {};                                           \
	DEVICE_DT_INST_DEFINE(n, init, NULL, &inst_##n##_data, &inst_##n##_config, POST_KERNEL,    \
			      CONFIG_I2C_INIT_PRIORITY, &adxl355_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(ADXL355_INIT)