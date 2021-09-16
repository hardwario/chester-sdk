#include <hio_chester_x0d.h>
#include <hio_bsp.h>

// Zephyr includes
#include <device.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_chester_x0d, LOG_LEVEL_DBG);

#define INPUT_1_DEV_A HIO_BSP_GP0A_DEV
#define INPUT_1_PIN_A HIO_BSP_GP0A_PIN
#define INPUT_2_DEV_A HIO_BSP_GP1A_DEV
#define INPUT_2_PIN_A HIO_BSP_GP1A_PIN
#define INPUT_3_DEV_A HIO_BSP_GP2A_DEV
#define INPUT_3_PIN_A HIO_BSP_GP2A_PIN
#define INPUT_4_DEV_A HIO_BSP_GP3A_DEV
#define INPUT_4_PIN_A HIO_BSP_GP3A_PIN

#define INPUT_1_DEV_B HIO_BSP_GP0B_DEV
#define INPUT_1_PIN_B HIO_BSP_GP0B_PIN
#define INPUT_2_DEV_B HIO_BSP_GP1B_DEV
#define INPUT_2_PIN_B HIO_BSP_GP1B_PIN
#define INPUT_3_DEV_B HIO_BSP_GP2B_DEV
#define INPUT_3_PIN_B HIO_BSP_GP2B_PIN
#define INPUT_4_DEV_B HIO_BSP_GP3B_DEV
#define INPUT_4_PIN_B HIO_BSP_GP3B_PIN

struct input {
    enum hio_chester_x0d_slot slot;
    enum hio_chester_x0d_channel channel;
    const char *device_name;
    int pin;
    hio_chester_x0d_event_cb event_callback;
    void *event_param;
    struct gpio_callback gpio_callback_rising;
    struct gpio_callback gpio_callback_falling;
};

static struct input m_inputs[2][4] = {
    [HIO_CHESTER_X0D_SLOT_A] = {
        [HIO_CHESTER_X0D_CHANNEL_1] = {
            .slot = HIO_CHESTER_X0D_SLOT_A,
            .channel = HIO_CHESTER_X0D_CHANNEL_1,
            .device_name = INPUT_1_DEV_A,
            .pin = INPUT_1_PIN_A
        },
        [HIO_CHESTER_X0D_CHANNEL_2] = {
            .slot = HIO_CHESTER_X0D_SLOT_A,
            .channel = HIO_CHESTER_X0D_CHANNEL_2,
            .device_name = INPUT_2_DEV_A,
            .pin = INPUT_2_PIN_A
        },
        [HIO_CHESTER_X0D_CHANNEL_3] = {
            .slot = HIO_CHESTER_X0D_SLOT_A,
            .channel = HIO_CHESTER_X0D_CHANNEL_3,
            .device_name = INPUT_3_DEV_A,
            .pin = INPUT_3_PIN_A
        },
        [HIO_CHESTER_X0D_CHANNEL_4] = {
            .slot = HIO_CHESTER_X0D_SLOT_A,
            .channel = HIO_CHESTER_X0D_CHANNEL_4,
            .device_name = INPUT_4_DEV_A,
            .pin = INPUT_4_PIN_A
        }
    },
    [HIO_CHESTER_X0D_SLOT_B] = {
        [HIO_CHESTER_X0D_CHANNEL_1] = {
            .slot = HIO_CHESTER_X0D_SLOT_B,
            .channel = HIO_CHESTER_X0D_CHANNEL_1,
            .device_name = INPUT_1_DEV_B,
            .pin = INPUT_1_PIN_B
        },
        [HIO_CHESTER_X0D_CHANNEL_2] = {
            .slot = HIO_CHESTER_X0D_SLOT_B,
            .channel = HIO_CHESTER_X0D_CHANNEL_2,
            .device_name = INPUT_2_DEV_B,
            .pin = INPUT_2_PIN_B
        },
        [HIO_CHESTER_X0D_CHANNEL_3] = {
            .slot = HIO_CHESTER_X0D_SLOT_B,
            .channel = HIO_CHESTER_X0D_CHANNEL_3,
            .device_name = INPUT_3_DEV_B,
            .pin = INPUT_3_PIN_B
        },
        [HIO_CHESTER_X0D_CHANNEL_4] = {
            .slot = HIO_CHESTER_X0D_SLOT_B,
            .channel = HIO_CHESTER_X0D_CHANNEL_4,
            .device_name = INPUT_4_DEV_B,
            .pin = INPUT_4_PIN_B
        }
    }
};

static void gpio_callback_rising_handler(const struct device *port,
                                         struct gpio_callback *cb,
                                         gpio_port_pins_t pins)
{
    struct input *input =
        CONTAINER_OF(cb, struct input, gpio_callback_rising);

    // TODO Call from k_work
    if (input->event_callback != NULL) {
        input->event_callback(input->slot, input->channel,
                              HIO_CHESTER_X0D_EVENT_RISING,
                              input->event_param);
    }
}

static void gpio_callback_falling_handler(const struct device *port,
                                          struct gpio_callback *cb,
                                          gpio_port_pins_t pins)
{
    struct input *input =
        CONTAINER_OF(cb, struct input, gpio_callback_falling);

    // TODO Call from k_work
    if (input->event_callback != NULL) {
        input->event_callback(input->slot, input->channel,
                              HIO_CHESTER_X0D_EVENT_FALLING,
                              input->event_param);
    }
}

static int setup(struct input *input)
{
    int ret;

    const struct device *dev = device_get_binding(input->device_name);

    if (dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        return -ENODEV;
    }

    ret = gpio_pin_configure(dev, input->pin, GPIO_INPUT);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
        return ret;
    }

    gpio_init_callback(&input->gpio_callback_rising,
                       gpio_callback_rising_handler, BIT(input->pin));

    ret = gpio_add_callback(dev, &input->gpio_callback_rising);

    if (ret < 0) {
        LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure(dev, input->pin, GPIO_INT_EDGE_RISING);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
        return ret;
    }

    gpio_init_callback(&input->gpio_callback_falling,
                       gpio_callback_falling_handler, BIT(input->pin));

    ret = gpio_add_callback(dev, &input->gpio_callback_falling);

    if (ret < 0) {
        LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure(dev, input->pin, GPIO_INT_EDGE_FALLING);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_chester_x0d_init(enum hio_chester_x0d_slot slot,
                         enum hio_chester_x0d_channel channel,
                         hio_chester_x0d_event_cb cb, void *param)
{
    int ret;

    struct input *input = &m_inputs[slot][channel];

    input->event_callback = cb;
    input->event_param = param;

    ret = setup(input);

    if (ret < 0) {
        LOG_ERR("Call `setup` failed: %d", ret);
        return ret;
    }

    return 0;
}
