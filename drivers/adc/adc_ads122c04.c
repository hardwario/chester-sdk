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

#define CONFIG_0_PGA_BYPASS_pos 0
#define CONFIG_0_PGA_BYPASS_msk BIT(0)
#define CONFIG_0_GAIN_pos 1
#define CONFIG_0_GAIN_msk BIT(1) | BIT(2) | BIT(3)
#define CONFIG_0_MUX_pos 4
#define CONFIG_0_MUX_msk BIT(4) | BIT(5) | BIT(6) | BIT(7)

#define CONFIG_1_TS_pos 0
#define CONFIG_1_TS_msk BIT(0)
#define CONFIG_1_VREF_pos 1
#define CONFIG_1_VREF_msk BIT(1) | BIT(2)
#define CONFIG_1_CM_pos 3
#define CONFIG_1_CM_msk BIT(3)
#define CONFIG_1_MODE_pos 4
#define CONFIG_1_MODE_msk BIT(4)
#define CONFIG_1_DR_pos 5
#define CONFIG_1_DR_msk BIT(5) | BIT(6) | BIT(7)

#define CONFIG_2_IDAC_pos 0
#define CONFIG_2_IDAC_msk BIT(0) | BIT(1) | BIT(2)
#define CONFIG_2_BCS_pos 3
#define CONFIG_2_BCS_msk BIT(3)
#define CONFIG_2_CRC_pos 4
#define CONFIG_2_CRC_msk BIT(4) | BIT(5)
#define CONFIG_2_DCNT_pos 6
#define CONFIG_2_DCNT_msk BIT(6)
#define CONFIG_2_DRDY_pos 7
#define CONFIG_2_DRDY_msk BIT(7)

#define CONFIG_2_I2MUX_pos 2
#define CONFIG_2_I2MUX_msk BIT(2) | BIT(3) | BIT(4)
#define CONFIG_2_I1MUX_pos 5
#define CONFIG_2_I1MUX_msk BIT(5) | BIT(6) | BIT(7)

#define RREG_ADDR_SHIFT 2

struct ads122c04_config {
	const struct i2c_dt_spec i2c_spec;
};

struct ads122c04_data {
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

static inline int rdata(const struct device *dev, uint8_t *data, uint8_t len)
{
	return read(dev, CMD_RDATA, data, len);
}

static inline int rreg(const struct device *dev, uint8_t addr, uint8_t *reg)
{
	return read(dev, CMD_RREG | (addr << RREG_ADDR_SHIFT), reg, 1);
}

static inline int wreg(const struct device *dev, uint8_t addr, uint8_t reg)
{
	return write(dev, CMD_WREG | (addr << RREG_ADDR_SHIFT), &reg, 1);
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

	uint8_t reg;
	do {
		ret = rreg(dev, REG_CONFIG_2, &reg);
		if (ret) {
			LOG_ERR("Call `rreg` failed: %d", ret);
			return ret;
		}
	} while (!(reg & CONFIG_2_DRDY_msk));

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

	ret = wreg(dev, REG_CONFIG_0, 0x30);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_1, 0x02);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_2, 0x06);
	if (ret) {
		LOG_ERR("Call `wreg` failed: %d", ret);
		return ret;
	}

	ret = wreg(dev, REG_CONFIG_3, 0x80);
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
	};                                                                                         \
	static struct ads122c04_data inst_##n##_data;                                              \
	DEVICE_DT_INST_DEFINE(n, ads122c04_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
	                      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ads122c04_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADS122C04_INIT)
