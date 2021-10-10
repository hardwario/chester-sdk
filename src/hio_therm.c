#include <hio_therm.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_therm, LOG_LEVEL_DBG);

int hio_therm_init(void)
{
    int ret;

    const struct device *dev = device_get_binding("TMP112");

    if (dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        return -ENODEV;
    }

    struct sensor_value val;

    val.val1 = 128;
    val.val2 = 0;

    ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP,
                          SENSOR_ATTR_FULL_SCALE, &val);

    if (ret < 0) {
        LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
        return ret;
    }

    val.val1 = 0;
    val.val2 = 250000;

    ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &val);

    if (ret < 0) {
        LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_therm_read(float *temperature)
{
    int ret;

    const struct device *dev = device_get_binding("TMP112");

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
        LOG_ERR("Call `sensor_channel_get` failed:t %d", ret);
        return ret;
    }

    float tmp_temperature = sensor_value_to_double(&val);;

    LOG_DBG("Temperature: %.2f C", tmp_temperature);

    if (temperature != NULL) {
        *temperature = tmp_temperature;
    }

    return 0;
}
