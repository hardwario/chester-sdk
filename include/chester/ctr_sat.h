#ifndef CHESTER_INCLUDE_CTR_SAT_H_
#define CHESTER_INCLUDE_CTR_SAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
// TODO: Compute more accurate
// Longest message WIF_W
#define TX_MESSAGE_MAX_SIZE 820
#else
// TODO: Compute more accurate.
// Longest message PLD_E
#define TX_MESSAGE_MAX_SIZE 400
#endif

#define RX_MESSAGE_MAX_SIZE 1000

#define MAX_PAYLOADS 8

typedef uint16_t message_handle;

enum ctr_sat_event {
	CTR_SAT_EVENT_MESSAGE_SENT = 0,
	CTR_SAT_EVENT_MESSAGE_RECEIVED = 1,
};

struct ctr_sat_event_msg_sent_data {
	message_handle msg;
};

struct ctr_sat_event_msg_recv_data {
	void *data;
	size_t data_len;
};

union ctr_sat_event_data {
	struct ctr_sat_event_msg_sent_data msg_send;
	struct ctr_sat_event_msg_recv_data msg_recv;
};

typedef void (*ctr_sat_message_event_cb)(enum ctr_sat_event event, union ctr_sat_event_data *data,
					 void *user_data);

struct ctr_sat {
	const struct device *uart_dev;

	struct k_poll_signal rx_completed_signal;
	atomic_t is_started;
	struct k_mutex mutex;

	uint8_t tx_buf[TX_MESSAGE_MAX_SIZE];
	size_t tx_buf_len;
	uint8_t *tx_buf_transmit_ptr;

	uint8_t rx_buf[RX_MESSAGE_MAX_SIZE];
	size_t rx_buf_len;
	uint8_t *rx_buf_receive_ptr;

	ctr_sat_message_event_cb callback;
	void *callback_user_data;

	uint16_t enqued_payload_ids[MAX_PAYLOADS];
	uint16_t enqued_payloads;
	uint16_t last_payload_id;

	struct gpio_dt_spec module_reset_gpio;
	struct gpio_dt_spec module_wakeup_gpio;
	struct gpio_dt_spec module_event_gpio;

	struct gpio_callback event_cb;

	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_CTR_SAT_THREAD_STACK_SIZE);
	struct k_thread thread;
	struct k_sem event_trig_sem;
};

struct ctr_sat_hwcfg {
	const struct device *uart_dev;
	const struct gpio_dt_spec module_reset_gpio;
	const struct gpio_dt_spec module_wakeup_gpio;
	const struct gpio_dt_spec module_event_gpio;
};

#define CTR_SAT_HWCONFIG_SYSCON                                                                    \
	{                                                                                          \
		.uart_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740_syscon)),                  \
		.module_reset_gpio =                                                               \
			GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_reset_gpios),          \
		.module_wakeup_gpio =                                                              \
			GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_wakeup_gpios),         \
		.module_event_gpio =                                                               \
			GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_v1_syscon), modem_event_gpios),          \
	}

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_message_event_cb user_cb, void *user_data);
int ctr_sat_start(struct ctr_sat *sat, const struct ctr_sat_hwcfg *hwcfg);
int ctr_sat_stop(struct ctr_sat *sat);
int ctr_sat_send_message(struct ctr_sat *sat, message_handle *msg_handle, const void *buf,
			 size_t len);
int ctr_sat_flush_message(struct ctr_sat *sat, message_handle msg_handle);
int ctr_sat_flush_messages(struct ctr_sat *sat);
int ctr_sat_use_wifi_devkit(struct ctr_sat *sat, const char *ssid, const char *password,
			    const char *api_key);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_SAT_H_ */
