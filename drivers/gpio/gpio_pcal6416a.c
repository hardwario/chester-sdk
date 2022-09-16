#define DT_DRV_COMPAT nxp_pcal6416a

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "gpio_utils.h"

LOG_MODULE_REGISTER(pcal6416a, CONFIG_GPIO_LOG_LEVEL);

enum {
	PCAL6416A_REG_INPUT_PORT_P0 = 0x00,
	PCAL6416A_REG_INPUT_PORT_P1 = 0x01,
	PCAL6416A_REG_OUTPUT_PORT_P0 = 0x02,
	PCAL6416A_REG_OUTPUT_PORT_P1 = 0x03,
	PCAL6416A_REG_POLARITY_INVERSION_P0 = 0x04,
	PCAL6416A_REG_POLARITY_INVERSION_P1 = 0x05,
	PCAL6416A_REG_CONFIGURATION_P0 = 0x06,
	PCAL6416A_REG_CONFIGURATION_P1 = 0x07,
	PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P0_0 = 0x40,
	PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P0_1 = 0x41,
	PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P1_0 = 0x42,
	PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P1_1 = 0x43,
	PCAL6416A_REG_INPUT_LATCH_P0 = 0x44,
	PCAL6416A_REG_INPUT_LATCH_P1 = 0x45,
	PCAL6416A_REG_PULL_UP_DOWN_ENABLE_P0 = 0x46,
	PCAL6416A_REG_PULL_UP_DOWN_ENABLE_P1 = 0x47,
	PCAL6416A_REG_PULL_UP_DOWN_SELECT_P0 = 0x48,
	PCAL6416A_REG_PULL_UP_DOWN_SELECT_P1 = 0x49,
	PCAL6416A_REG_INTERRUPT_MASK_P0 = 0x4a,
	PCAL6416A_REG_INTERRUPT_MASK_P1 = 0x4b,
	PCAL6416A_REG_INTERRUPT_STATUS_P0 = 0x4c,
	PCAL6416A_REG_INTERRUPT_STATUS_P1 = 0x4d,
	PCAL6416A_REG_OUTPUT_PORT_CONFIGURATION = 0x4f,
};

struct pcal6416a_pins_cfg {
	uint8_t configured_as_inputs_p0;
	uint8_t configured_as_inputs_p1;
	uint8_t outputs_high_p0;
	uint8_t outputs_high_p1;
	uint8_t pull_ups_selected_p0;
	uint8_t pull_ups_selected_p1;
	uint8_t pulls_enabled_p0;
	uint8_t pulls_enabled_p1;
};

struct pcal6416a_triggers {
	uint8_t masked_p0;
	uint8_t masked_p1;
	uint8_t dual_edge_p0;
	uint8_t dual_edge_p1;
	uint8_t on_low_p0;
	uint8_t on_low_p1;
};

struct pcal6416a_drv_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;

	sys_slist_t callbacks;
	struct k_sem lock;
	struct k_work work;
	const struct device *dev;
	struct gpio_callback int_gpio_cb;
	struct pcal6416a_pins_cfg pins_cfg;
	struct pcal6416a_triggers triggers;
	uint8_t input_port_last_p0;
	uint8_t input_port_last_p1;
};

struct pcal6416a_drv_cfg {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;

	const struct device *i2c;
	uint16_t i2c_addr;
	const struct device *int_gpio_dev;
	gpio_pin_t int_gpio_pin;
	gpio_dt_flags_t int_gpio_flags;
	const struct device *reset_gpio_dev;
	gpio_pin_t reset_gpio_pin;
	gpio_dt_flags_t reset_gpio_flags;
};

static int pcal6416a_pins_cfg_apply(const struct device *dev, struct pcal6416a_pins_cfg pins_cfg)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	int rc;

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
	                        PCAL6416A_REG_PULL_UP_DOWN_SELECT_P0,
	                        pins_cfg.pull_ups_selected_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to select pull-up/pull-down resistors: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
	                        PCAL6416A_REG_PULL_UP_DOWN_SELECT_P1,
	                        pins_cfg.pull_ups_selected_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to select pull-up/pull-down resistors: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
	                        PCAL6416A_REG_PULL_UP_DOWN_ENABLE_P0, pins_cfg.pulls_enabled_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to enable pull-up/pull-down resistors: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
	                        PCAL6416A_REG_PULL_UP_DOWN_ENABLE_P1, pins_cfg.pulls_enabled_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to enable pull-up/pull-down resistors: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_OUTPUT_PORT_P0,
	                        pins_cfg.outputs_high_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to set outputs: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_OUTPUT_PORT_P1,
	                        pins_cfg.outputs_high_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to set outputs: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_CONFIGURATION_P0,
	                        pins_cfg.configured_as_inputs_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to configure pins: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_CONFIGURATION_P1,
	                        pins_cfg.configured_as_inputs_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to configure pins: %d", dev->name, rc);
		return -EIO;
	}

	return 0;
}

static int pcal6416a_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	struct pcal6416a_drv_data *drv_data = dev->data;
	struct pcal6416a_pins_cfg pins_cfg;
	gpio_flags_t flags_io;
	int rc;

	/* This device does not support open-source outputs, and open-drain
	 * outputs can be only configured port-wise.
	 */
	if ((flags & GPIO_SINGLE_ENDED) != 0) {
		return -ENOTSUP;
	}

	/* Pins in this device can be either inputs or outputs and cannot be
	 * completely disconnected.
	 */
	flags_io = (flags & (GPIO_INPUT | GPIO_OUTPUT));
	if (flags_io == (GPIO_INPUT | GPIO_OUTPUT) || flags_io == GPIO_DISCONNECTED) {
		return -ENOTSUP;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&drv_data->lock, K_FOREVER);

	pins_cfg = drv_data->pins_cfg;

	if ((flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0) {
		if ((flags & GPIO_PULL_UP) != 0) {
			if (pin < 8) {
				pins_cfg.pull_ups_selected_p0 |= BIT(pin);
			} else {
				pins_cfg.pull_ups_selected_p1 |= BIT(pin - 8);
			}
		} else {
			if (pin < 8) {
				pins_cfg.pull_ups_selected_p0 &= ~BIT(pin);
			} else {
				pins_cfg.pull_ups_selected_p1 &= ~BIT(pin - 8);
			}
		}

		if (pin < 8) {
			pins_cfg.pulls_enabled_p0 |= BIT(pin);
		} else {
			pins_cfg.pulls_enabled_p1 |= BIT(pin - 8);
		}
	} else {
		if (pin < 8) {
			pins_cfg.pulls_enabled_p0 &= ~BIT(pin);
		} else {
			pins_cfg.pulls_enabled_p1 &= ~BIT(pin - 8);
		}
	}

	if ((flags & GPIO_OUTPUT) != 0) {
		if ((flags & GPIO_OUTPUT_INIT_LOW) != 0) {
			if (pin < 8) {
				pins_cfg.outputs_high_p0 &= ~BIT(pin);
			} else {
				pins_cfg.outputs_high_p1 &= ~BIT(pin - 8);
			}
		} else if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0) {
			if (pin < 8) {
				pins_cfg.outputs_high_p0 |= BIT(pin);
			} else {
				pins_cfg.outputs_high_p1 |= BIT(pin - 8);
			}
		}

		if (pin < 8) {
			pins_cfg.configured_as_inputs_p0 &= ~BIT(pin);
		} else {
			pins_cfg.configured_as_inputs_p1 &= ~BIT(pin - 8);
		}
	} else {
		if (pin < 8) {
			pins_cfg.configured_as_inputs_p0 |= BIT(pin);
		} else {
			pins_cfg.configured_as_inputs_p1 |= BIT(pin - 8);
		}
	}

	rc = pcal6416a_pins_cfg_apply(dev, pins_cfg);
	if (rc == 0) {
		drv_data->pins_cfg = pins_cfg;
	}

	k_sem_give(&drv_data->lock);

	return rc;
}

static int pcal6416a_process_input(const struct device *dev, gpio_port_value_t *value)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	struct pcal6416a_drv_data *drv_data = dev->data;
	int rc;
	uint8_t int_sources_p0;
	uint8_t int_sources_p1;
	uint8_t input_port_p0;
	uint8_t input_port_p1;

	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INTERRUPT_STATUS_P0,
	                       &int_sources_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to read interrupt sources: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INTERRUPT_STATUS_P1,
	                       &int_sources_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to read interrupt sources: %d", dev->name, rc);
		return -EIO;
	}

	/* This read also clears the generated interrupt if any. */
	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_PORT_P0,
	                       &input_port_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to read input port: %d", dev->name, rc);
		return -EIO;
	}

	/* This read also clears the generated interrupt if any. */
	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_PORT_P1,
	                       &input_port_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to read input port: %d", dev->name, rc);
		return -EIO;
	}

	if (value) {
		*value = input_port_p0 | input_port_p1 << 8;
	}

	/* It may happen that some inputs change their states between above
	 * reads of the interrupt status and input port registers. Such changes
	 * will not be noted in `int_sources`, thus to correctly detect them,
	 * the current state of inputs needs to be additionally compared with
	 * the one read last time, and any differences need to be added to
	 * `int_sources`.
	 */
	int_sources_p0 |=
	        ((input_port_p0 ^ drv_data->input_port_last_p0) & ~drv_data->triggers.masked_p0);
	int_sources_p1 |=
	        ((input_port_p1 ^ drv_data->input_port_last_p1) & ~drv_data->triggers.masked_p1);

	drv_data->input_port_last_p0 = input_port_p0;
	drv_data->input_port_last_p1 = input_port_p1;

	if (int_sources_p0 || int_sources_p1) {
		uint8_t dual_edge_triggers_p0 = drv_data->triggers.dual_edge_p0;
		uint8_t dual_edge_triggers_p1 = drv_data->triggers.dual_edge_p1;
		uint8_t falling_edge_triggers_p0 =
		        (~dual_edge_triggers_p0 & drv_data->triggers.on_low_p0);
		uint8_t falling_edge_triggers_p1 =
		        (~dual_edge_triggers_p1 & drv_data->triggers.on_low_p1);
		uint8_t fired_triggers_p0 = 0;
		uint8_t fired_triggers_p1 = 0;

		/* For dual edge triggers, react to all state changes. */
		fired_triggers_p0 |= (int_sources_p0 & dual_edge_triggers_p0);
		fired_triggers_p1 |= (int_sources_p1 & dual_edge_triggers_p1);
		/* For single edge triggers, fire callbacks only for the pins
		 * that transitioned to their configured target state (0 for
		 * falling edges, 1 otherwise, hence the XOR operation below).
		 */
		fired_triggers_p0 |= ((input_port_p0 & int_sources_p0) ^ falling_edge_triggers_p0);
		fired_triggers_p1 |= ((input_port_p1 & int_sources_p1) ^ falling_edge_triggers_p1);

		gpio_fire_callbacks(&drv_data->callbacks, dev,
		                    fired_triggers_p0 | fired_triggers_p1 << 8);
	}

	return 0;
}

static void pcal6416a_work_handler(struct k_work *work)
{
	struct pcal6416a_drv_data *drv_data = CONTAINER_OF(work, struct pcal6416a_drv_data, work);

	k_sem_take(&drv_data->lock, K_FOREVER);

	(void)pcal6416a_process_input(drv_data->dev, NULL);

	k_sem_give(&drv_data->lock);
}

static void pcal6416a_int_gpio_handler(const struct device *dev, struct gpio_callback *gpio_cb,
                                       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	struct pcal6416a_drv_data *drv_data =
	        CONTAINER_OF(gpio_cb, struct pcal6416a_drv_data, int_gpio_cb);

	k_work_submit(&drv_data->work);
}

static int pcal6416a_port_get_raw(const struct device *dev, gpio_port_value_t *value)
{
	struct pcal6416a_drv_data *drv_data = dev->data;
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&drv_data->lock, K_FOREVER);

	/* Reading of the input port also clears the generated interrupt,
	 * thus the configured callbacks must be fired also here if needed.
	 */
	rc = pcal6416a_process_input(dev, value);

	k_sem_give(&drv_data->lock);

	return rc;
}

static int pcal6416a_port_set_raw(const struct device *dev, uint16_t mask, uint16_t value,
                                  uint16_t toggle)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	struct pcal6416a_drv_data *drv_data = dev->data;
	int rc;
	uint8_t mask_p0 = mask;
	uint8_t mask_p1 = mask >> 8;
	uint8_t value_p0 = value;
	uint8_t value_p1 = value >> 8;
	uint8_t toggle_p0 = toggle;
	uint8_t toggle_p1 = toggle >> 8;
	uint8_t output_p0;
	uint8_t output_p1;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&drv_data->lock, K_FOREVER);

	output_p0 = drv_data->pins_cfg.outputs_high_p0 & ~mask_p0;
	output_p1 = drv_data->pins_cfg.outputs_high_p1 & ~mask_p1;
	output_p0 |= value_p0 & mask_p0;
	output_p1 |= value_p1 & mask_p1;
	output_p0 ^= toggle_p0;
	output_p1 ^= toggle_p1;

	/*
	 * No need to limit `out` to only pins configured as outputs,
	 * as the chip anyway ignores all other bits in the register.
	 */
	do {
		rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
		                        PCAL6416A_REG_OUTPUT_PORT_P0, output_p0);
		if (rc == 0) {
			drv_data->pins_cfg.outputs_high_p0 = output_p0;
		} else {
			break;
		}

		rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr,
		                        PCAL6416A_REG_OUTPUT_PORT_P1, output_p1);
		if (rc == 0) {
			drv_data->pins_cfg.outputs_high_p1 = output_p1;
		} else {
			break;
		}
	} while (0);

	k_sem_give(&drv_data->lock);

	if (rc != 0) {
		LOG_ERR("%s: failed to write output port: %d", dev->name, rc);
		return -EIO;
	}

	return 0;
}

static int pcal6416a_port_set_masked_raw(const struct device *dev, gpio_port_pins_t mask,
                                         gpio_port_value_t value)
{
	return pcal6416a_port_set_raw(dev, (uint16_t)mask, (uint16_t)value, 0);
}

static int pcal6416a_port_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	return pcal6416a_port_set_raw(dev, (uint16_t)pins, (uint16_t)pins, 0);
}

static int pcal6416a_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	return pcal6416a_port_set_raw(dev, (uint16_t)pins, 0, 0);
}

static int pcal6416a_port_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
	return pcal6416a_port_set_raw(dev, 0, 0, (uint16_t)pins);
}

static int pcal6416a_triggers_apply(const struct device *dev, struct pcal6416a_triggers triggers)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	int rc;

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_LATCH_P0,
	                        ~(triggers.masked_p0));
	if (rc != 0) {
		LOG_ERR("%s: failed to configure input latch: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_LATCH_P1,
	                        ~(triggers.masked_p1));
	if (rc != 0) {
		LOG_ERR("%s: failed to configure input latch: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INTERRUPT_MASK_P0,
	                        triggers.masked_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to configure interrupt mask: %d", dev->name, rc);
		return -EIO;
	}

	rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INTERRUPT_MASK_P1,
	                        triggers.masked_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to configure interrupt mask: %d", dev->name, rc);
		return -EIO;
	}

	return 0;
}

static int pcal6416a_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
                                             enum gpio_int_mode mode, enum gpio_int_trig trig)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	struct pcal6416a_drv_data *drv_data = dev->data;
	struct pcal6416a_triggers triggers;
	int rc;

	if (!drv_cfg->int_gpio_dev) {
		return -ENOTSUP;
	}

	/* This device supports only edge-triggered interrupts. */
	if (mode == GPIO_INT_MODE_LEVEL) {
		return -ENOTSUP;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&drv_data->lock, K_FOREVER);

	triggers = drv_data->triggers;

	if (mode == GPIO_INT_MODE_DISABLED) {
		if (pin < 8) {
			triggers.masked_p0 |= BIT(pin);
		} else {
			triggers.masked_p1 |= BIT(pin - 8);
		}

	} else {
		if (pin < 8) {
			triggers.masked_p0 &= ~BIT(pin);
		} else {
			triggers.masked_p1 &= ~BIT(pin - 8);
		}
	}

	if (trig == GPIO_INT_TRIG_BOTH) {
		if (pin < 8) {
			triggers.dual_edge_p0 |= BIT(pin);
		} else {
			triggers.dual_edge_p1 |= BIT(pin - 8);
		}
	} else {
		if (pin < 8) {
			triggers.dual_edge_p0 &= ~BIT(pin);
		} else {
			triggers.dual_edge_p1 &= ~BIT(pin - 8);
		}

		if (trig == GPIO_INT_TRIG_LOW) {
			if (pin < 8) {
				triggers.on_low_p0 |= BIT(pin);
			} else {
				triggers.on_low_p1 |= BIT(pin - 8);
			}
		} else {
			if (pin < 8) {
				triggers.on_low_p0 &= ~BIT(pin);
			} else {
				triggers.on_low_p1 &= ~BIT(pin - 8);
			}
		}
	}

	rc = pcal6416a_triggers_apply(dev, triggers);
	if (rc == 0) {
		drv_data->triggers = triggers;
	}

	k_sem_give(&drv_data->lock);

	return rc;
}

static int pcal6416a_manage_callback(const struct device *dev, struct gpio_callback *callback,
                                     bool set)
{
	struct pcal6416a_drv_data *drv_data = dev->data;

	return gpio_manage_callback(&drv_data->callbacks, callback, set);
}

static int pcal6416a_init(const struct device *dev)
{
	const struct pcal6416a_drv_cfg *drv_cfg = dev->config;
	struct pcal6416a_drv_data *drv_data = dev->data;
	const struct pcal6416a_pins_cfg initial_pins_cfg = {
		.configured_as_inputs_p0 = 0xff,
		.configured_as_inputs_p1 = 0xff,
		.outputs_high_p0 = 0,
		.outputs_high_p1 = 0,
		.pull_ups_selected_p0 = 0,
		.pull_ups_selected_p1 = 0,
		.pulls_enabled_p0 = 0,
		.pulls_enabled_p1 = 0,
	};
	const struct pcal6416a_triggers initial_triggers = {
		.masked_p0 = 0xff,
		.masked_p1 = 0xff,
	};
	int rc;

	if (!device_is_ready(drv_cfg->i2c)) {
		LOG_ERR("%s is not ready", drv_cfg->i2c->name);
		return -ENODEV;
	}

	/* If the RESET line is available, use it to reset the expander.
	 * Otherwise, write reset values to registers that are not used by
	 * this driver.
	 */
	if (drv_cfg->reset_gpio_dev) {
		if (!device_is_ready(drv_cfg->reset_gpio_dev)) {
			LOG_ERR("%s is not ready", drv_cfg->reset_gpio_dev->name);
			return -ENODEV;
		}

		rc = gpio_pin_configure(drv_cfg->reset_gpio_dev, drv_cfg->reset_gpio_pin,
		                        drv_cfg->reset_gpio_flags | GPIO_OUTPUT_ACTIVE);
		if (rc != 0) {
			LOG_ERR("%s: failed to configure RESET line: %d", dev->name, rc);
			return -EIO;
		}

		/* RESET signal needs to be active for a minimum of 30 ns. */
		k_busy_wait(1);

		rc = gpio_pin_set(drv_cfg->reset_gpio_dev, drv_cfg->reset_gpio_pin, 0);
		if (rc != 0) {
			LOG_ERR("%s: failed to deactivate RESET line: %d", dev->name, rc);
			return -EIO;
		}

		/* Give the expander at least 200 ns to recover after reset. */
		k_busy_wait(1);
	} else {
		static const uint8_t reset_state[][2] = {
			{ PCAL6416A_REG_POLARITY_INVERSION_P0, 0 },
			{ PCAL6416A_REG_POLARITY_INVERSION_P1, 0 },
			{ PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P0_0, 0xff },
			{ PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P0_1, 0xff },
			{ PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P1_0, 0xff },
			{ PCAL6416A_REG_OUTPUT_DRIVE_STRENGTH_P1_1, 0xff },
			{ PCAL6416A_REG_OUTPUT_PORT_CONFIGURATION, 0 },
		};

		for (int i = 0; i < ARRAY_SIZE(reset_state); ++i) {
			rc = i2c_reg_write_byte(drv_cfg->i2c, drv_cfg->i2c_addr, reset_state[i][0],
			                        reset_state[i][1]);
			if (rc != 0) {
				LOG_ERR("%s: failed to reset register %02x: %d", dev->name,
				        reset_state[i][0], rc);
				return -EIO;
			}
		}
	}

	/* Set initial configuration of the pins. */
	rc = pcal6416a_pins_cfg_apply(dev, initial_pins_cfg);
	if (rc != 0) {
		return rc;
	}

	drv_data->pins_cfg = initial_pins_cfg;

	/* Read initial state of the input port register. */
	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_PORT_P0,
	                       &drv_data->input_port_last_p0);
	if (rc != 0) {
		LOG_ERR("%s: failed to initially read input port: %d", dev->name, rc);
		return -EIO;
	}

	/* Read initial state of the input port register. */
	rc = i2c_reg_read_byte(drv_cfg->i2c, drv_cfg->i2c_addr, PCAL6416A_REG_INPUT_PORT_P1,
	                       &drv_data->input_port_last_p1);
	if (rc != 0) {
		LOG_ERR("%s: failed to initially read input port: %d", dev->name, rc);
		return -EIO;
	}

	/* Set initial state of the interrupt related registers. */
	rc = pcal6416a_triggers_apply(dev, initial_triggers);
	if (rc != 0) {
		return rc;
	}

	drv_data->triggers = initial_triggers;

	/* If the INT line is available, configure the callback for it. */
	if (drv_cfg->int_gpio_dev) {
		if (!device_is_ready(drv_cfg->int_gpio_dev)) {
			LOG_ERR("%s is not ready", drv_cfg->int_gpio_dev->name);
			return -ENODEV;
		}

		rc = gpio_pin_configure(drv_cfg->int_gpio_dev, drv_cfg->int_gpio_pin,
		                        drv_cfg->int_gpio_flags | GPIO_INPUT);
		if (rc != 0) {
			LOG_ERR("%s: failed to configure INT line: %d", dev->name, rc);
			return -EIO;
		}

		rc = gpio_pin_interrupt_configure(drv_cfg->int_gpio_dev, drv_cfg->int_gpio_pin,
		                                  GPIO_INT_EDGE_TO_ACTIVE);
		if (rc != 0) {
			LOG_ERR("%s: failed to configure INT interrupt: %d", dev->name, rc);
			return -EIO;
		}

		gpio_init_callback(&drv_data->int_gpio_cb, pcal6416a_int_gpio_handler,
		                   BIT(drv_cfg->int_gpio_pin));
		rc = gpio_add_callback(drv_cfg->int_gpio_dev, &drv_data->int_gpio_cb);
		if (rc != 0) {
			LOG_ERR("%s: failed to add INT callback: %d", dev->name, rc);
			return -EIO;
		}
	}

	/* Device configured, unlock it so that it can be used. */
	k_sem_give(&drv_data->lock);

	return 0;
}

static const struct gpio_driver_api pcal6416a_drv_api = {
	.pin_configure = pcal6416a_pin_configure,
	.port_get_raw = pcal6416a_port_get_raw,
	.port_set_masked_raw = pcal6416a_port_set_masked_raw,
	.port_set_bits_raw = pcal6416a_port_set_bits_raw,
	.port_clear_bits_raw = pcal6416a_port_clear_bits_raw,
	.port_toggle_bits = pcal6416a_port_toggle_bits,
	.pin_interrupt_configure = pcal6416a_pin_interrupt_configure,
	.manage_callback = pcal6416a_manage_callback,
};

#define INIT_INT_GPIO_FIELDS(idx)                                                                  \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(idx, int_gpios),                                         \
	            (.int_gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(idx), int_gpios)),     \
	             .int_gpio_pin = DT_INST_GPIO_PIN(idx, int_gpios),                             \
	             .int_gpio_flags = DT_INST_GPIO_FLAGS(idx, int_gpios), ),                      \
	            ())

#define INIT_RESET_GPIO_FIELDS(idx)                                                                \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(idx, reset_gpios),                                       \
	            (.reset_gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(idx), reset_gpios)), \
	             .reset_gpio_pin = DT_INST_GPIO_PIN(idx, reset_gpios),                         \
	             .reset_gpio_flags = DT_INST_GPIO_FLAGS(idx, reset_gpios), ),                  \
	            ())

#define GPIO_PCAL6416A_INST(idx)                                                                   \
	static const struct pcal6416a_drv_cfg pcal6416a_cfg##idx = {	   \
		.common = {						   \
			.port_pin_mask =				   \
				GPIO_PORT_PIN_MASK_FROM_DT_INST(idx),	   \
		},							   \
		.i2c = DEVICE_DT_GET(DT_INST_BUS(idx)),			   \
		.i2c_addr = DT_INST_REG_ADDR(idx),			   \
		INIT_INT_GPIO_FIELDS(idx)				   \
		INIT_RESET_GPIO_FIELDS(idx)				   \
	};                          \
	static struct pcal6416a_drv_data pcal6416a_data##idx = {                                   \
		.lock = Z_SEM_INITIALIZER(pcal6416a_data##idx.lock, 0, 1),                         \
		.work = Z_WORK_INITIALIZER(pcal6416a_work_handler),                                \
		.dev = DEVICE_DT_INST_GET(idx),                                                    \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(idx, pcal6416a_init, NULL, &pcal6416a_data##idx,                     \
	                      &pcal6416a_cfg##idx, POST_KERNEL,                                    \
	                      CONFIG_GPIO_PCAL6416A_INIT_PRIORITY, &pcal6416a_drv_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_PCAL6416A_INST)
