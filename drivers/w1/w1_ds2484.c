/* Zephyr includes */
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/w1.h>
#include <logging/log.h>
#include <pm/device.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

#define DT_DRV_COMPAT maxim_ds2484

#define CMD_DRST 0xf0
#define CMD_SRP 0xe1
#define CMD_WCFG 0xd2
#define CMD_ADJP 0xc3
#define CMD_1WRS 0xb4
#define CMD_1WSB 0x87
#define CMD_1WWB 0xa5
#define CMD_1WRB 0x96
#define CMD_1WT 0x78

#define REG_NONE 0x00
#define REG_STATUS 0xf0
#define REG_DEVICE_CONFIG 0xc3
#define REG_PORT_CONFIG 0xb4
#define REG_DATA 0xe1

/*
 * Bit Byte
 */
#define BIT_pos 7
#define BIT_SET_msk BIT(BIT_pos)
#define BIT_CLR_msk 0

/*
 * Control Byte
 */
#define CTRL_VAL0_pos 0
#define CTRL_VAL0_msk BIT(CTRL_VAL0_pos)
#define CTRL_VAL1_pos 1
#define CTRL_VAL1_msk BIT(CTRL_VAL1_pos)
#define CTRL_VAL2_pos 2
#define CTRL_VAL2_msk BIT(CTRL_VAL2_pos)
#define CTRL_VAL3_pos 3
#define CTRL_VAL3_msk BIT(CTRL_VAL3_pos)
#define CTRL_OD_pos 4
#define CTRL_OD_msk BIT(CTRL_OD_pos)
#define CTRL_P0_pos 5
#define CTRL_P0_msk BIT(CTRL_P0_pos)
#define CTRL_P1_pos 6
#define CTRL_P1_msk BIT(CTRL_P1_pos)
#define CTRL_P2_pos 7
#define CTRL_P2_msk BIT(CTRL_P2_pos)

/*
 * Direction Byte
 */
#define DIR_V_pos 7
#define DIR_V_msk BIT(DIR_V_pos)

/*
 * Device Configuration Register
 */
#define DEVICE_APU_pos 0
#define DEVICE_APU_msk BIT(DEVICE_APU_pos)
#define DEVICE_PDN_pos 1
#define DEVICE_PDN_msk BIT(DEVICE_PDN_pos)
#define DEVICE_SPU_pos 2
#define DEVICE_SPU_msk BIT(DEVICE_SPU_pos)
#define DEVICE_1WS_pos 3
#define DEVICE_1WS_msk BIT(DEVICE_1WS_pos)

/*
 * Port Configuration Register
 */
#define PORT_VAL0_pos 0
#define PORT_VAL0_msk BIT(PORT_VAL0_pos)
#define PORT_VAL1_pos 1
#define PORT_VAL1_msk BIT(PORT_VAL1_pos)
#define PORT_VAL2_pos 2
#define PORT_VAL2_msk BIT(PORT_VAL2_pos)
#define PORT_VAL3_pos 3
#define PORT_VAL3_msk BIT(PORT_VAL3_pos)

/*
 * Status Register
 */
#define STATUS_1WB_pos 0
#define STATUS_1WB_msk BIT(STATUS_1WB_pos)
#define STATUS_PPD_pos 1
#define STATUS_PPD_msk BIT(STATUS_PPD_pos)
#define STATUS_SD_pos 2
#define STATUS_SD_msk BIT(STATUS_SD_pos)
#define STATUS_LL_pos 3
#define STATUS_LL_msk BIT(STATUS_LL_pos)
#define STATUS_RST_pos 4
#define STATUS_RST_msk BIT(STATUS_RST_pos)
#define STATUS_SBR_pos 5
#define STATUS_SBR_msk BIT(STATUS_SBR_pos)
#define STATUS_TSB_pos 6
#define STATUS_TSB_msk BIT(STATUS_TSB_pos)
#define STATUS_DIR_pos 7
#define STATUS_DIR_msk BIT(STATUS_DIR_pos)

LOG_MODULE_REGISTER(ds2484, CONFIG_W1_LOG_LEVEL);

struct ds2484_config {
	struct w1_controller_config w1_config;
	const struct i2c_dt_spec i2c_spec;
	const struct gpio_dt_spec slpz_spec;
	bool apu;
};

struct ds2484_data {
	struct k_mutex lock;
	uint8_t reg_device;
};

static inline const struct ds2484_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ds2484_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int exec(const struct device *dev, uint8_t cmd, uint8_t *data)
{
	int ret;

	const uint8_t tmp[] = { cmd, data ? *data : 0 };

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, tmp, data ? 2 : 1);
	if (ret) {
		LOG_ERR("Call `i2c_write_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int read(const struct device *dev, uint8_t rp, uint8_t *reg)
{
	int ret;

	switch (rp) {
	case REG_NONE:
		break;
	case REG_STATUS:
	case REG_DEVICE_CONFIG:
	case REG_PORT_CONFIG:
	case REG_DATA:
		ret = exec(dev, CMD_SRP, &rp);
		if (ret) {
			LOG_ERR("Call `exec` failed: %d", ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_read_dt(&get_config(dev)->i2c_spec, reg, 1);
	if (ret) {
		LOG_ERR("Call `i2c_read_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static bool ds2484_reset_bus(const struct device *dev)
{
	int ret;

	uint8_t reg;

	ret = exec(dev, CMD_1WRS, NULL);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return false;
	}

	do {
		ret = read(dev, REG_NONE, &reg);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return false;
		}
	} while (reg & STATUS_1WB_msk);

	return reg & STATUS_PPD_msk;
}

static bool single_bit(const struct device *dev, bool set)
{
	int ret;

	uint8_t reg = set ? BIT_SET_msk : BIT_CLR_msk;

	ret = exec(dev, CMD_1WSB, &reg);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return false;
	}

	do {
		ret = read(dev, REG_NONE, &reg);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}
	} while (reg & STATUS_1WB_msk);

	return reg & STATUS_SBR_msk;
}

static bool ds2484_read_bit(const struct device *dev)
{
	return single_bit(dev, true);
}

static void ds2484_write_bit(const struct device *dev, bool set)
{
	single_bit(dev, set);
}

static uint8_t ds2484_read_byte(const struct device *dev)
{
	int ret;

	uint8_t reg;

	ret = exec(dev, CMD_1WRB, NULL);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return 0;
	}

	do {
		ret = read(dev, REG_NONE, &reg);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return 0;
		}
	} while (reg & STATUS_1WB_msk);

	ret = read(dev, REG_DATA, &reg);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return 0;
	}

	return reg;
}

static void ds2484_write_byte(const struct device *dev, uint8_t byte)
{
	int ret;

	uint8_t reg;

	ret = exec(dev, CMD_1WWB, &byte);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return;
	}

	do {
		ret = read(dev, REG_NONE, &reg);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return;
		}
	} while (reg & STATUS_1WB_msk);
}

static void ds2484_read_block(const struct device *dev, uint8_t *buffer, size_t length)
{
	for (int i = 0; i < length; ++i) {
		buffer[i] = ds2484_read_byte(dev);
	}
}

static void ds2484_write_block(const struct device *dev, const uint8_t *buffer, size_t length)
{
	for (int i = 0; i < length; ++i) {
		ds2484_write_byte(dev, buffer[i]);
	}
}

static void ds2484_lock_bus(const struct device *dev)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);
}

static void ds2484_unlock_bus(const struct device *dev)
{
	k_mutex_unlock(&get_data(dev)->lock);
}

#ifdef CONFIG_PM_DEVICE
static int ds2484_pm_control(const struct device *dev, enum pm_device_action action)
{
	int ret;

	uint8_t reg;

	if (!device_is_ready(get_config(dev)->slpz_spec.port)) {
		LOG_ERR("Port SLPZ not ready");
		return -EINVAL;
	}

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		ret = gpio_pin_set_dt(&get_config(dev)->slpz_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			return ret;
		}
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		ds2484_lock_bus(dev);
		do {
			ret = read(dev, REG_STATUS, &reg);
			if (ret) {
				LOG_ERR("Call `read` failed: %d", ret);
				k_mutex_unlock(&get_data(dev)->lock);
				return ret;
			}
		} while (reg & STATUS_1WB_msk);
		ds2484_unlock_bus(dev);
	case PM_DEVICE_ACTION_FORCE_SUSPEND:
		ret = gpio_pin_set_dt(&get_config(dev)->slpz_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			return ret;
		}
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static int reset_device(const struct device *dev)
{
	int ret;

	uint8_t reg;

	ret = exec(dev, CMD_DRST, NULL);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return ret;
	}

	ret = read(dev, REG_NONE, &reg);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	return (reg & STATUS_RST_msk) ? 0 : -EIO;
}

static int update_device_config(const struct device *dev)
{
	int ret;

	uint8_t reg = get_data(dev)->reg_device;
	reg |= ~reg << 4;

	ret = exec(dev, CMD_WCFG, &reg);
	if (ret) {
		LOG_ERR("Call `exec` failed: %d", ret);
		return ret;
	}

	ret = read(dev, REG_NONE, &reg);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Device configuration: 0x%02X", reg);

	return (reg == get_data(dev)->reg_device) ? 0 : -EIO;
}

static int ds2484_init(const struct device *dev)
{
	int ret;

	k_mutex_init(&get_data(dev)->lock);

	if (!device_is_ready(get_config(dev)->slpz_spec.port)) {
		LOG_ERR("Port SLPZ not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->slpz_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin SLPZ configuration failed: %d", ret);
		return ret;
	}

	ret = reset_device(dev);
	if (ret) {
		LOG_ERR("Device reset failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_device, DEVICE_APU_pos, get_config(dev)->apu);

	ret = update_device_config(dev);
	if (ret) {
		LOG_ERR("Device config update failed: %d", ret);
		return ret;
	}

	LOG_DBG("Initialized with %d device(s))", get_config(dev)->w1_config.client_count);

	return 0;
}

static const struct w1_driver_api ds2484_driver_api = {
	.reset_bus = ds2484_reset_bus,
	.read_bit = ds2484_read_bit,
	.write_bit = ds2484_write_bit,
	.read_byte = ds2484_read_byte,
	.write_byte = ds2484_write_byte,
	.read_block = ds2484_read_block,
	.write_block = ds2484_write_block,
	.lock_bus = ds2484_lock_bus,
	.unlock_bus = ds2484_unlock_bus,
};

/*
 * Count the number of peripheral devices expected on the bus.
 * This can be used to decide if the bus has a multi-drop topology or
 * only a single device is present.
 * There is a comma after each ordinal (including the last)
 * Hence FOR_EACH adds "+1" once too often which has to be subtracted in the end.
 */
#define F1(x) 1
#define W1_INST_PERIPHERALS_COUNT(n) (FOR_EACH (F1, (+), DT_INST_SUPPORTS_DEP_ORDS(n)) - 1)

#define DS2484_INIT(n)                                                                             \
	static const struct ds2484_config inst_##n##_config = {                                    \
		.w1_config.client_count = W1_INST_PERIPHERALS_COUNT(n),                            \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.slpz_spec = GPIO_DT_SPEC_INST_GET(n, slpz_gpios),                                 \
		.apu = DT_INST_PROP(n, active_pullup),                                             \
	};                                                                                         \
	static struct ds2484_data inst_##n##_data = {};                                            \
	PM_DEVICE_DT_INST_DEFINE(n, ds2484_pm_control);                                            \
	DEVICE_DT_INST_DEFINE(n, ds2484_init, PM_DEVICE_DT_INST_REF(n), &inst_##n##_data,          \
	                      &inst_##n##_config, POST_KERNEL, CONFIG_W1_INIT_PRIORITY,            \
	                      &ds2484_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DS2484_INIT)
