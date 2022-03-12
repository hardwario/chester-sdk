#include <devicetree.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/util_macro.h>
#include <zephyr.h>

#define DT_DRV_COMPAT ti_ads122c04

LOG_MODULE_REGISTER(ads122c04, CONFIG_ADC_LOG_LEVEL);

#define CMD_RESET 0x06
#define CMD_START_SYNC 0x08
#define CMD_POWERDOWN 0x02
#define CMD_RDATA 0x10
#define CMD_RREG 0x20
#define CMD_WREG 0x40

#define REG_CONFIG_0 0x00
#define REG_CONFIG_1 0x01
#define REG_CONFIG_2 0x02
#define REG_CONFIG_3 0x03

#define DEF_CONFIG_0 0x00
#define DEF_CONFIG_1 0x00
#define DEF_CONFIG_2 0x00
#define DEF_CONFIG_3 0x00

#define POS_CONFIG_0_PGA_BYPASS 0
#define POS_CONFIG_0_GAIN 1
#define POS_CONFIG_0_MUX 4
#define POS_CONFIG_1_TS 0
#define POS_CONFIG_1_VREF 1
#define POS_CONFIG_1_CM 3
#define POS_CONFIG_1_MODE 4
#define POS_CONFIG_1_DR 5
#define POS_CONFIG_2_IDAC 0
#define POS_CONFIG_2_BCS 3
#define POS_CONFIG_2_CRC 4
#define POS_CONFIG_2_DCNT 6
#define POS_CONFIG_2_DRDY 7
#define POS_CONFIG_3_I2MUX 2
#define POS_CONFIG_3_I1MUX 5

#define MSK_CONFIG_0_MUX (BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define MSK_CONFIG_0_PGA_BYPASS BIT(0)
#define MSK_CONFIG_0_GAIN (BIT(1) | BIT(2) | BIT(3))
#define MSK_CONFIG_1_TS BIT(0)
#define MSK_CONFIG_1_VREF (BIT(1) | BIT(2))
#define MSK_CONFIG_1_CM BIT(3)
#define MSK_CONFIG_1_MODE BIT(4)
#define MSK_CONFIG_1_DR (BIT(5) | BIT(6) | BIT(7))
#define MSK_CONFIG_2_IDAC (BIT(0) | BIT(1) | BIT(2))
#define MSK_CONFIG_2_BCS BIT(3)
#define MSK_CONFIG_2_CRC (BIT(4) | BIT(5))
#define MSK_CONFIG_2_DCNT BIT(6)
#define MSK_CONFIG_2_DRDY BIT(7)
#define MSK_CONFIG_3_I2MUX (BIT(2) | BIT(3) | BIT(4))
#define MSK_CONFIG_3_I1MUX (BIT(5) | BIT(6) | BIT(7))

#define READ_MAX_RETRIES 100

struct ads122c04_config {
	const struct i2c_dt_spec i2c_spec;
	int gain;
	int mux;
	int vref;
};

struct ads122c04_data {
	uint8_t reg_config_0;
	uint8_t reg_config_1;
	uint8_t reg_config_2;
	uint8_t reg_config_3;
};

static inline const struct ads122c04_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ads122c04_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read(const struct device *dev, uint8_t op, uint8_t *data, size_t len)
{
	int ret;

	uint8_t buf[len];

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, &op, sizeof(op), buf, len);
	if (ret) {
		return ret;
	}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	sys_memcpy_swap(data, buf, len);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	memcpy(data, buf, len);
#else
#error "Unknown byte order"
#endif

	return 0;
}

static int write(const struct device *dev, uint8_t op, const uint8_t *data, size_t len)
{
	int ret;

	uint8_t buf[1 + len];
	buf[0] = op;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	sys_memcpy_swap(buf + 1, data, len);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	memcpy(buf + 1, data, len);
#else
#error "Unknown byte order"
#endif

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, buf, sizeof(buf));
	if (ret) {
		return ret;
	}

	return 0;
}

static inline int reset(const struct device *dev)
{
	return write(dev, CMD_RESET, NULL, 0);
}

static inline int start_sync(const struct device *dev)
{
	return write(dev, CMD_START_SYNC, NULL, 0);
}

static inline int powerdown(const struct device *dev)
{
	return write(dev, CMD_POWERDOWN, NULL, 0);
}

static inline int rdata(const struct device *dev, uint8_t *val, uint8_t len)
{
	return read(dev, CMD_RDATA, val, len);
}

static inline int rreg(const struct device *dev, uint8_t reg, uint8_t *val)
{
	return read(dev, CMD_RREG | reg << 2, val, 1);
}

static inline int wreg(const struct device *dev, uint8_t reg, uint8_t val)
{
	LOG_DBG("Reg: 0x%02x = 0x%02x", reg, val);

	return write(dev, CMD_WREG | reg << 2, &val, 1);
}

static int ads122c04_channel_setup(const struct device *dev,
                                   const struct adc_channel_cfg *channel_cfg)
{
	return 0;
}

static int ads122c04_read(const struct device *dev, const struct adc_sequence *sequence)
{
	int ret;

	int32_t *buffer = sequence->buffer;

	ret = start_sync(dev);
	if (ret) {
		LOG_ERR("Call `start_sync` failed: %d", ret);
		return ret;
	}

	int retries = 0;

	for (;;) {
		uint8_t val;
		ret = rreg(dev, REG_CONFIG_2, &val);
		if (ret) {
			LOG_ERR("Call `rreg` failed: %d", ret);
			return ret;
		}

		if (val & MSK_CONFIG_2_DRDY) {
			break;
		}

		if (++retries >= READ_MAX_RETRIES) {
			LOG_ERR("Reached maximum DRDY poll attempts");
			return -EIO;
		}

		k_msleep(10);
	}

	uint8_t buf[3] = { 0 };
	ret = rdata(dev, buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `rdata` failed: %d", ret);
		return ret;
	}

	ret = powerdown(dev);
	if (ret) {
		LOG_ERR("Call `powerdown` failed: %d", ret);
		return ret;
	}

	buffer[0] = (buf[2] << 24 | buf[1] << 16 | buf[0] << 8) / 256;

	return 0;
}

static int ads122c04_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	ret = reset(dev);
	if (ret) {
		LOG_ERR("Call `reset` failed: %d", ret);
		return ret;
	}

	get_data(dev)->reg_config_0 = DEF_CONFIG_0;
	get_data(dev)->reg_config_0 |= get_config(dev)->gain << POS_CONFIG_0_GAIN;
	get_data(dev)->reg_config_0 |= get_config(dev)->mux << POS_CONFIG_0_MUX;
	get_data(dev)->reg_config_1 = DEF_CONFIG_1;
	get_data(dev)->reg_config_1 |= get_config(dev)->vref << POS_CONFIG_1_VREF;
	get_data(dev)->reg_config_2 = DEF_CONFIG_2;
	get_data(dev)->reg_config_3 = DEF_CONFIG_3;

	ret = wreg(dev, REG_CONFIG_0, get_data(dev)->reg_config_0);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_1, get_data(dev)->reg_config_1);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_2, get_data(dev)->reg_config_2);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_3, get_data(dev)->reg_config_3);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct adc_driver_api ads122c04_driver_api = {
	.channel_setup = ads122c04_channel_setup,
	.read = ads122c04_read,
	.ref_internal = 4097 /* [-2048, 2048] mV */,
};

#define ADS122C04_INIT(n)                                                                          \
	static const struct ads122c04_config inst_##n##_config = {                                 \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.gain = DT_INST_PROP(n, gain),                                                     \
		.mux = DT_INST_PROP(n, mux),                                                       \
		.vref = DT_INST_PROP(n, vref),                                                     \
	};                                                                                         \
	static struct ads122c04_data inst_##n##_data;                                              \
	DEVICE_DT_INST_DEFINE(n, ads122c04_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
	                      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ads122c04_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADS122C04_INIT)
