#define DT_DRV_COMPAT adi_adxl355

/* Chester includes */
#include <chester/drivers/adxl355.h>

/* Zephyre includes */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ADXL355_TRIGGER, CONFIG_ADXL355_LOG_LEVEL);

static void adxl355_thread_cb(const struct device *dev)
{
	// struct adxl355_data *data = dev->data;
	uint8_t status_buf;

	/* Clears activity and inactivity interrupt */
	if (adxl355_get_status(dev, &status_buf)) {
		LOG_ERR("Unable to get status.");
		return;
	}
}

static void adxl355_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	struct adxl355_data *data = dev->data;
	k_work_submit(&data->work);
#endif
}

#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
static void adxl355_work_cb(struct k_work *work)
{
	struct adxl355_data *data = CONTAINER_OF(work, struct adxl355_data, work);

	adxl355_thread_cb(data->dev);
}

#endif

int adxl355_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler)
{
	const struct adxl355_config *config = dev->config;
	struct adxl355_data *data = dev->data;
	uint8_t int_mask, int_en, status;
	int ret;

	switch (trig->type) {

	case SENSOR_TRIG_DATA_READY:
		k_mutex_lock(&data->trigger_mutex, K_FOREVER);

#if defined(CONFIG_ADXL355_TRIGGER)
		if (device_is_ready(config->int1.port)) {
			data->handler_int1 = handler;
			data->trigger_int1 = *trig;
			LOG_INF("trigger drdy is initialize to gpio '%d'", config->int1.pin);
		} else {
			LOG_ERR("is not maped any gpio pin");
		}

		ret = adxl355_get_status(dev, &status);
		if (ret) {
			LOG_ERR("Call 'adxl355_get_satatus' failed: %d", ret);
			return ret;
		}
#endif
		k_mutex_unlock(&data->trigger_mutex);
		int_mask = 0x2A; // ADXL355_INTMAP1_DATA_READY;
		break;
	default:
		LOG_ERR("Unsupported sensor trigger");
		return -ENOTSUP;
	}

	if (handler) {
		int_en = int_mask;
	} else {
		int_en = 0U;
	}

	// ret adxl55_write_mask(dev, ADXL355_REG_INTMAP1, int_mask, int_en);

	return 0;
}

static int adxl355_init_interrupt_gpio(const struct device *dev, const struct gpio_dt_spec *gpio,
				       struct gpio_callback *cb)
{
	int ret;

	if (!device_is_ready(gpio->port)) {
		LOG_DBG("GPIO port %s not ready", gpio->port->name);
	} else {
		ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
		if (ret < 0) {
			return ret;
		}

		gpio_init_callback(cb, adxl355_gpio_callback, BIT(gpio->pin));
		ret = gpio_add_callback(gpio->port, cb);
		if (ret < 0) {
			return ret;
		}
		ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_TO_ACTIVE);
		if (ret) {
			return ret;
		}
	}

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
	ret = adxl355_init_interrupt_gpio(dev, &config->drdy, &data->gpio_cb_drdy);
	/* init int1 */
	ret = adxl355_init_interrupt_gpio(dev, &config->int1, &data->gpio_cb_int1);
	/* init int2 */
	ret = adxl355_init_interrupt_gpio(dev, &config->int2, &data->gpio_cb_int2);

	data->dev = dev;

#if defined(CONFIG_ADXL355_TRIGGER_GLOBAL_THREAD)
	data->work.handler = adxl355_work_cb;
#endif

	return 0;
}