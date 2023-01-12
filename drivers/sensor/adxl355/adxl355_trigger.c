#define DT_DRV_COMPAT adi_adxl355

/* Chester includes */
#include <chester/drivers/adxl355.h>

/* Zephyre includes */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ADXL355_TRIGGER, CONFIG_ADXL355_LOG_LEVEL);

static void adxl355_thread_cb_drdy(const struct device *dev)
{
	struct adxl355_data *data = dev->data;
	uint8_t status_buf;

	k_mutex_lock(&data->trigger_mutex, K_FOREVER);
	if (data->handler_drdy != NULL) {
		data->handler_drdy(dev, &data->trigger_drdy);
	}

	k_mutex_unlock(&data->trigger_mutex);

	/* Clears activity and inactivity interrupt */
	if (adxl355_get_status(dev, &status_buf)) {
		LOG_ERR("Unable to get status.");
		return;
	} else {
		LOG_DBG("status: 0x%02x", status_buf);
	}
}

static void adxl355_thread_cb_int1(const struct device *dev)
{
	struct adxl355_data *data = dev->data;
	uint8_t status_buf;

	k_mutex_lock(&data->trigger_mutex, K_FOREVER);
	if (data->handler_int1 != NULL) {
		data->handler_int1(dev, &data->trigger_int1);
	}

	k_mutex_unlock(&data->trigger_mutex);

	/* Clears activity and inactivity interrupt */
	if (adxl355_get_status(dev, &status_buf)) {
		LOG_ERR("Unable to get status.");
		return;
	} else {
		LOG_DBG("status: 0x%02x", status_buf);
	}
}

static void adxl355_thread_cb_int2(const struct device *dev)
{
	struct adxl355_data *data = dev->data;
	uint8_t status_buf;

	k_mutex_lock(&data->trigger_mutex, K_FOREVER);
	if (data->handler_int2 != NULL) {
		data->handler_int2(dev, &data->trigger_int2);
	}

	k_mutex_unlock(&data->trigger_mutex);

	/* Clears activity and inactivity interrupt */
	if (adxl355_get_status(dev, &status_buf)) {
		LOG_ERR("Unable to get status.");
		return;
	} else {
		LOG_DBG("status: 0x%02x", status_buf);
	}
}

static void gpio_handler_drdy(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct adxl355_data *data = CONTAINER_OF(cb, struct adxl355_data, gpio_cb_drdy);
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work_drdy);
#endif
}

static void gpio_handler_int1(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct adxl355_data *data = CONTAINER_OF(cb, struct adxl355_data, gpio_cb_int1);
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work_int1);
#endif
}

static void gpio_handler_int2(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct adxl355_data *data = CONTAINER_OF(cb, struct adxl355_data, gpio_cb_int2);
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work_int2);
#endif
}

#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
static void adxl355_work_cb_drdy(struct k_work *work)
{
	struct adxl355_data *data = CONTAINER_OF(work, struct adxl355_data, work_drdy);
	adxl355_thread_cb_drdy(data->dev);
}

static void adxl355_work_cb_int1(struct k_work *work)
{
	struct adxl355_data *data = CONTAINER_OF(work, struct adxl355_data, work_int1);
	adxl355_thread_cb_int1(data->dev);
}

static void adxl355_work_cb_int2(struct k_work *work)
{
	struct adxl355_data *data = CONTAINER_OF(work, struct adxl355_data, work_int2);
	adxl355_thread_cb_int2(data->dev);
}

#endif

int adxl355_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler)
{
	const struct adxl355_config *config = dev->config;
	struct adxl355_data *data = dev->data;
	int ret;

	switch (trig->type) {

	case SENSOR_TRIG_DATA_READY:
		k_mutex_lock(&data->trigger_mutex, K_FOREVER);

#if defined(CONFIG_ADXL355_TRIGGER)
		if (device_is_ready(config->drdy.port)) {
			data->handler_drdy = handler;
			data->trigger_drdy = *trig;
			LOG_DBG("trigger drdy is initialize to gpio drdy: '%d'", config->drdy.pin);
		} else {
			LOG_DBG("drdy is not maped any gpio pin");
		}

#if defined(CONFIG_ADXL355_INT1_DRDY) || defined(CONFIG_ADXL355_INT1_FIFO_FULL)
		if (device_is_ready(config->int1.port)) {
			data->handler_int1 = handler;
			data->trigger_int1 = *trig;
			LOG_DBG("trigger is initialize to gpio int1: '%d'", config->int1.pin);
		} else {
			LOG_DBG("int1 is not maped any gpio pin");
		}
#elif defined(CONFIG_ADXL355_INT2_DRDY) || defined(CONFIG_ADXL355_INT2_FIFO_FULL)
		if (device_is_ready(config->int2.port)) {
			data->handler_int2 = handler;
			data->trigger_int2 = *trig;
			LOG_DBG("trigger is initialize to gpio int2: '%d'", config->int2.pin);
		} else {
			LOG_DBG("int2 is not maped any gpio pin");
		}
#endif

		/* clear interrupts*/
		ret = adxl355_clear_interrupts(dev);
		if (ret) {
			LOG_ERR("Call 'adxl355_clear_interrupts' failed: %d", ret);
			k_mutex_unlock(&data->trigger_mutex);
			return ret;
		}

#endif
		k_mutex_unlock(&data->trigger_mutex);
		break;
	default:
		LOG_ERR("Unsupported sensor trigger");
		return -ENOTSUP;
	}

	return 0;
}

static int init_interrupt_gpio(const struct device *dev, const struct gpio_dt_spec *gpio,
			       gpio_callback_handler_t handler, struct gpio_callback *cb)
{
	if (!device_is_ready(gpio->port)) {
		LOG_DBG("GPIO port %d not ready", gpio->pin);
		return -ENODEV;
	} else {
		int ret;

		ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
		if (ret < 0) {
			LOG_ERR("Call 'gpio_init_callback' failed: %d", ret);
			return ret;
		}
		/* gpio initialize callback */
		gpio_init_callback(cb, handler, BIT(gpio->pin));

		ret = gpio_add_callback(gpio->port, cb);
		if (ret) {
			LOG_ERR("Call 'gpio_add_Callback' failed: %d", ret);
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_TO_ACTIVE);
		if (ret) {
			LOG_ERR("Call 'gpio_pin_interrupt_configure_dt' failed: %d", ret);
			return ret;
		}
	}

	LOG_DBG("GPIO %d successfully initialized to trigger callback", gpio->pin);

	return 0;
}

int adxl355_init_interrupts(const struct device *dev)
{
	LOG_INF("Interupts initialization");
	const struct adxl355_config *config = dev->config;
	struct adxl355_data *data = dev->data;
	int ret;

	k_mutex_init(&data->trigger_mutex);

	/* Configure used interupts pins */
	/* init drdy */
	ret = init_interrupt_gpio(dev, &config->drdy, gpio_handler_drdy, &data->gpio_cb_drdy);
	/* init int1 */
	ret = init_interrupt_gpio(dev, &config->int1, gpio_handler_int1, &data->gpio_cb_int1);
	if (!ret) {
#if defined(CONFIG_ADXL355_INT1_DRDY)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_DRDY_EN1_MSK,
					 ADXL355_INT_MAP_DRDY_EN1_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT1_FIFO_FULL)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_FULL_EN1_MSK,
					 ADXL355_INT_MAP_FULL_EN1_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT1_FIFO_OVR)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_OVR_EN1_MSK,
					 ADXL355_INT_MAP_OVR_EN1_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT1_ACT)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_ACT_EN1_MSK,
					 ADXL355_INT_MAP_ACT_EN1_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
	}
	/* init int2 */
	ret = init_interrupt_gpio(dev, &config->int2, gpio_handler_int2, &data->gpio_cb_int2);
	if (!ret) {
#if defined(CONFIG_ADXL355_INT2_DRDY)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_DRDY_EN2_MSK,
					 ADXL355_INT_MAP_DRDY_EN2_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT2_FIFO_FULL)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_FULL_EN2_MSK,
					 ADXL355_INT_MAP_FULL_EN2_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT2_FIFO_OVR)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_OVR_EN2_MSK,
					 ADXL355_INT_MAP_OVR_EN2_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
#if defined(CONFIG_ADXL355_INT2_ACT)
		ret = adxl355_write_mask(dev, ADXL355_INT_MAP_REG, ADXL355_INT_MAP_ACT_EN2_MSK,
					 ADXL355_INT_MAP_ACT_EN2_MODE(0x01));
		if (ret) {
			LOG_ERR("Call 'write_mask' failed: %d", ret);
		}
#endif
	}

	data->dev = dev;

#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	data->work_drdy.handler = adxl355_work_cb_drdy;
	data->work_int1.handler = adxl355_work_cb_int1;
	data->work_int2.handler = adxl355_work_cb_int2;
#endif

	LOG_INF("successfully initialized");

	return 0;
}