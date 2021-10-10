#include <hio_hygro.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_hygro, LOG_LEVEL_DBG);

int hio_hygro_init(void)
{
    return 0;
}

int hio_hygro_read(float *temperature, float *humidity)
{
    int ret;

    const struct device *dev = device_get_binding("SHT30");

    if (dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        return -ENODEV;
    }

    ret = sensor_sample_fetch(dev);

    if (ret < 0) {
        LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
        return ret;
    }

    struct sensor_value val;

    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);

    if (ret < 0) {
        LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
        return ret;
    }

    float tmp_temperature = sensor_value_to_double(&val);

    LOG_DBG("Temperature: %.2f C", tmp_temperature);

    if (temperature != NULL) {
        *temperature = tmp_temperature;
    }

    ret = sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &val);

    if (ret < 0) {
        LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
        return ret;
    }

    float tmp_humidity = sensor_value_to_double(&val);

    LOG_DBG("Humidity: %.1f %%", tmp_humidity);

    if (humidity != NULL) {
        *humidity = tmp_humidity;
    }

    return 0;
}
