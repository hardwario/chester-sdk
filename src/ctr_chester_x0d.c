#include <ctr_chester_x0d.h>

/* Zephyr includes */
#include <device.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_chester_x0d, LOG_LEVEL_DBG);

/* TODO Remove this after K_MEM_SLAB_DEFINE_STATIC definition makes it to mainline */
#define K_MEM_SLAB_DEFINE_STATIC(name, slab_block_size, slab_num_blocks, slab_align)               \
	static char __noinit __aligned(WB_UP(slab_align))                                          \
	        _k_mem_slab_buf_##name[(slab_num_blocks)*WB_UP(slab_block_size)];                  \
	static STRUCT_SECTION_ITERABLE(k_mem_slab, name) = Z_MEM_SLAB_INITIALIZER(                 \
	        name, _k_mem_slab_buf_##name, WB_UP(slab_block_size), slab_num_blocks)

#define EVENT_WORK_NAME "chester_x0d"
#define EVENT_WORK_STACK_SIZE 2048
#define EVENT_SLAB_MAX_ITEMS 64

#define INPUT_1_DEV_A DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_1_PIN_A 3
#define INPUT_2_DEV_A DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_2_PIN_A 29
#define INPUT_3_DEV_A DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_3_PIN_A 2
#define INPUT_4_DEV_A DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_4_PIN_A 31

#define INPUT_1_DEV_B DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_1_PIN_B 28
#define INPUT_2_DEV_B DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_2_PIN_B 30
#define INPUT_3_DEV_B DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_3_PIN_B 4
#define INPUT_4_DEV_B DT_LABEL(DT_NODELABEL(gpio0))
#define INPUT_4_PIN_B 5

struct input {
	bool initialized;
	enum ctr_chester_x0d_slot slot;
	enum ctr_chester_x0d_channel channel;
	const char *device_name;
	int pin;
	ctr_chester_x0d_event_cb event_callback;
	void *event_param;
	struct gpio_callback gpio_callback;
	bool rising;
};

static struct input m_inputs[2][4] = {
	[CTR_CHESTER_X0D_SLOT_A] = { [CTR_CHESTER_X0D_CHANNEL_1] = { .slot = CTR_CHESTER_X0D_SLOT_A,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_1,
	                                                             .device_name = INPUT_1_DEV_A,
	                                                             .pin = INPUT_1_PIN_A },
	                             [CTR_CHESTER_X0D_CHANNEL_2] = { .slot = CTR_CHESTER_X0D_SLOT_A,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_2,
	                                                             .device_name = INPUT_2_DEV_A,
	                                                             .pin = INPUT_2_PIN_A },
	                             [CTR_CHESTER_X0D_CHANNEL_3] = { .slot = CTR_CHESTER_X0D_SLOT_A,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_3,
	                                                             .device_name = INPUT_3_DEV_A,
	                                                             .pin = INPUT_3_PIN_A },
	                             [CTR_CHESTER_X0D_CHANNEL_4] = { .slot = CTR_CHESTER_X0D_SLOT_A,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_4,
	                                                             .device_name = INPUT_4_DEV_A,
	                                                             .pin = INPUT_4_PIN_A } },
	[CTR_CHESTER_X0D_SLOT_B] = { [CTR_CHESTER_X0D_CHANNEL_1] = { .slot = CTR_CHESTER_X0D_SLOT_B,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_1,
	                                                             .device_name = INPUT_1_DEV_B,
	                                                             .pin = INPUT_1_PIN_B },
	                             [CTR_CHESTER_X0D_CHANNEL_2] = { .slot = CTR_CHESTER_X0D_SLOT_B,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_2,
	                                                             .device_name = INPUT_2_DEV_B,
	                                                             .pin = INPUT_2_PIN_B },
	                             [CTR_CHESTER_X0D_CHANNEL_3] = { .slot = CTR_CHESTER_X0D_SLOT_B,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_3,
	                                                             .device_name = INPUT_3_DEV_B,
	                                                             .pin = INPUT_3_PIN_B },
	                             [CTR_CHESTER_X0D_CHANNEL_4] = { .slot = CTR_CHESTER_X0D_SLOT_B,
	                                                             .channel =
	                                                                     CTR_CHESTER_X0D_CHANNEL_4,
	                                                             .device_name = INPUT_4_DEV_B,
	                                                             .pin = INPUT_4_PIN_B } }
};

struct event_item {
	struct k_work work;
	enum ctr_chester_x0d_slot slot;
	enum ctr_chester_x0d_channel channel;
	enum ctr_chester_x0d_event event;
	ctr_chester_x0d_event_cb callback;
	void *param;
};

K_MEM_SLAB_DEFINE_STATIC(m_event_slab, sizeof(struct event_item), EVENT_SLAB_MAX_ITEMS, 4);
static K_THREAD_STACK_DEFINE(m_event_work_stack_area, EVENT_WORK_STACK_SIZE);
static struct k_work_q m_event_work_q;
static K_MUTEX_DEFINE(m_mut);

static void event_work_handler(struct k_work *work)
{
	struct event_item *ei = CONTAINER_OF(work, struct event_item, work);

	if (ei->callback != NULL) {
		ei->callback(ei->slot, ei->channel, ei->event, ei->param);
	}

	k_mem_slab_free(&m_event_slab, (void **)&ei);
}

static void gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
                                  gpio_port_pins_t pins)
{
	int ret;

	struct input *input = CONTAINER_OF(cb, struct input, gpio_callback);

	const struct device *dev = device_get_binding(input->device_name);

	if (dev == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		k_oops();
	}

	enum ctr_chester_x0d_event event =
	        input->rising ? CTR_CHESTER_X0D_EVENT_RISE : CTR_CHESTER_X0D_EVENT_FALL;

	ret = gpio_pin_interrupt_configure(
	        dev, input->pin, input->rising ? GPIO_INT_EDGE_FALLING : GPIO_INT_EDGE_RISING);

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

	gpio_init_callback(&input->gpio_callback, gpio_callback_handler, BIT(input->pin));

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

int ctr_chester_x0d_init(enum ctr_chester_x0d_slot slot, enum ctr_chester_x0d_channel channel,
                         ctr_chester_x0d_event_cb callback, void *param)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	static bool initialized;

	if (!initialized) {
		struct k_work_queue_config config = { .name = EVENT_WORK_NAME, .no_yield = false };

		k_work_queue_start(&m_event_work_q, m_event_work_stack_area,
		                   K_THREAD_STACK_SIZEOF(m_event_work_stack_area),
		                   K_HIGHEST_APPLICATION_THREAD_PRIO, &config);

		initialized = true;
	}

	struct input *input = &m_inputs[slot][channel];

	if (input->initialized) {
		LOG_ERR("Input already initialized");
		k_mutex_unlock(&m_mut);
		return -EPERM;
	}

	input->event_callback = callback;
	input->event_param = param;

	ret = setup(input);

	if (ret < 0) {
		LOG_ERR("Call `setup` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}

int ctr_chester_x0d_read(enum ctr_chester_x0d_slot slot, enum ctr_chester_x0d_channel channel,
                         int *level)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	struct input *input = &m_inputs[slot][channel];

	if (!input->initialized) {
		LOG_ERR("Input not initialized");
		k_mutex_unlock(&m_mut);
		return -EPERM;
	}

	const struct device *dev = device_get_binding(input->device_name);

	if (dev == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	ret = gpio_pin_get(dev, input->pin);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	*level = ret;

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_channel_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	enum ctr_chester_x0d_slot slot;

	if (strcmp(argv[1], "a") == 0) {
		slot = CTR_CHESTER_X0D_SLOT_A;

	} else if (strcmp(argv[1], "b") == 0) {
		slot = CTR_CHESTER_X0D_SLOT_B;

	} else {
		shell_error(shell, "invalid slot");
		shell_help(shell);
		return -EINVAL;
	}

	enum ctr_chester_x0d_channel channel;

	if (strcmp(argv[2], "1") == 0) {
		channel = CTR_CHESTER_X0D_CHANNEL_1;

	} else if (strcmp(argv[2], "2") == 0) {
		channel = CTR_CHESTER_X0D_CHANNEL_2;

	} else if (strcmp(argv[2], "3") == 0) {
		channel = CTR_CHESTER_X0D_CHANNEL_3;

	} else if (strcmp(argv[2], "4") == 0) {
		channel = CTR_CHESTER_X0D_CHANNEL_4;

	} else {
		shell_error(shell, "invalid channel");
		shell_help(shell);
		return -EINVAL;
	}

	int level;

	ret = ctr_chester_x0d_read(slot, channel, &level);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "level: %d", level);

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_chester_x0d,
                               SHELL_CMD_ARG(read, NULL,
                                             "Read channel state"
                                             " (format a|b 1|2|3|4).",
                                             cmd_channel_read, 3, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(chester_x0d, &sub_chester_x0d, "CHESTER-X0D commands.", print_help);
