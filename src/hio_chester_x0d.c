#include <hio_chester_x0d.h>
#include <hio_bsp.h>

// Zephyr includes
#include <device.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_chester_x0d, LOG_LEVEL_DBG);

#define EVENT_WORK_NAME "chester_x0d"
#define EVENT_WORK_STACK_SIZE 1024
#define EVENT_SLAB_MAX_ITEMS 64

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
    bool initialized;
    enum hio_chester_x0d_slot slot;
    enum hio_chester_x0d_channel channel;
    const char *device_name;
    int pin;
    hio_chester_x0d_event_cb event_callback;
    void *event_param;
    struct gpio_callback gpio_callback;
    bool rising;
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

struct event_item {
    struct k_work work;
    enum hio_chester_x0d_slot slot;
    enum hio_chester_x0d_channel channel;
    enum hio_chester_x0d_event event;
    hio_chester_x0d_event_cb callback;
    void *param;
};

static K_MEM_SLAB_DEFINE(m_event_slab, sizeof(struct event_item),
                         EVENT_SLAB_MAX_ITEMS, 4);
static K_THREAD_STACK_DEFINE(m_event_work_stack_area, EVENT_WORK_STACK_SIZE);
static struct k_work_q m_event_work_q;

static void event_work_handler(struct k_work *work)
{
    struct event_item *ei = CONTAINER_OF(work, struct event_item, work);

    if (ei->callback != NULL) {
        ei->callback(ei->slot, ei->channel, ei->event, ei->param);
    }

    k_mem_slab_free(&m_event_slab, (void **)&ei);
}

static void gpio_callback_handler(const struct device *port,
                                  struct gpio_callback *cb,
                                  gpio_port_pins_t pins)
{
    int ret;

    struct input *input = CONTAINER_OF(cb, struct input, gpio_callback);

    const struct device *dev = device_get_binding(input->device_name);

    if (dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        k_oops();
    }

    enum hio_chester_x0d_event event = input->rising ?
        HIO_CHESTER_X0D_EVENT_RISE : HIO_CHESTER_X0D_EVENT_FALL;

    ret = gpio_pin_interrupt_configure(dev, input->pin, input->rising ?
                                       GPIO_INT_EDGE_FALLING :
                                       GPIO_INT_EDGE_RISING);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
        k_oops();
    }

    input->rising = !input->rising;

    struct event_item *ei;

    ret = k_mem_slab_alloc(&m_event_slab, (void **)&ei, K_NO_WAIT);

    if (ret < 0) {
        LOG_ERR("Call `k_mem_slab_alloc` failed: %d", ret);
        return;
    }

    ei->slot = input->slot;
    ei->channel = input->channel;
    ei->event = event;
    ei->callback = input->event_callback;
    ei->param = input->event_param;

    k_work_init(&ei->work, event_work_handler);

    ret = k_work_submit_to_queue(&m_event_work_q, &ei->work);

    if (ret < 0) {
        LOG_ERR("Call `k_work_submit_to_queue` failed: %d", ret);
        k_mem_slab_free(&m_event_slab, (void **)&ei);
        return;
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

    gpio_init_callback(&input->gpio_callback,
                       gpio_callback_handler, BIT(input->pin));

    ret = gpio_add_callback(dev, &input->gpio_callback);

    if (ret < 0) {
        LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure(dev, input->pin, GPIO_INT_EDGE_FALLING);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
        return ret;
    }

    input->initialized = true;

    return 0;
}

int hio_chester_x0d_init(enum hio_chester_x0d_slot slot,
                         enum hio_chester_x0d_channel channel,
                         hio_chester_x0d_event_cb callback, void *param)
{
    int ret;

    static bool initialized;

    if (!initialized) {
        struct k_work_queue_config config = {
            .name = EVENT_WORK_NAME,
            .no_yield = false
        };

        k_work_queue_start(&m_event_work_q, m_event_work_stack_area,
                           K_THREAD_STACK_SIZEOF(m_event_work_stack_area),
                           K_HIGHEST_APPLICATION_THREAD_PRIO, &config);

        initialized = true;
    }

    struct input *input = &m_inputs[slot][channel];

    if (input->initialized) {
        LOG_ERR("Input already initialized");
        return -EPERM;
    }

    input->event_callback = callback;
    input->event_param = param;

    ret = setup(input);

    if (ret < 0) {
        LOG_ERR("Call `setup` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_chester_x0d_read(enum hio_chester_x0d_slot slot,
                         enum hio_chester_x0d_channel channel, int *level)
{
    int ret;

    struct input *input = &m_inputs[slot][channel];

    if (!input->initialized) {
        LOG_ERR("Input not initialized");
        return -EPERM;
    }

    const struct device *dev = device_get_binding(input->device_name);

    if (dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        return -ENODEV;
    }

    ret = gpio_pin_get(dev, input->pin);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_get` failed: %d", ret);
        return ret;
    }

    *level = ret;
    return 0;
}
