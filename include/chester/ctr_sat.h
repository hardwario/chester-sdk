#ifndef CHESTER_INCLUDE_CTR_SAT_H_
#define CHESTER_INCLUDE_CTR_SAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Zephyr includes */
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/timeutil.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
// Longest request: WIF_W (194 bytes)
// 8 == 1 (start byte) + 2 (command) + 4 (CRC) + 1 (end byte)
#define TX_MESSAGE_MAX_SIZE (194 * 2 + 8)
#else
// Longest request: PLD_E (162 bytes)
// 8 == 1 (start byte) + 2 (command) + 4 (CRC) + 1 (end byte)
#define TX_MESSAGE_MAX_SIZE (162 * 2 + 8)
#endif /* CONFIG_CTR_SAT_USE_WIFI_DEVKIT */

// Longest answer: CMD_R (44 bytes)
// 8 == 1 (start byte) + 2 (command) + 4 (CRC) + 1 (end byte)
#define RX_MESSAGE_MAX_SIZE (44 * 2 + 8)

#define MAX_PAYLOADS 8

typedef uint16_t ctr_sat_msg_handle;

enum ctr_sat_event {
	CTR_SAT_EVENT_MESSAGE_SENT = 0,
	CTR_SAT_EVENT_MESSAGE_RECEIVED = 1,
};

struct ctr_sat_event_msg_sent_data {
	ctr_sat_msg_handle msg;
};

struct ctr_sat_event_msg_recv_data {
	void *data;
	size_t data_len;
	time_t created_at;
};

union ctr_sat_event_data {
	struct ctr_sat_event_msg_sent_data msg_send;
	struct ctr_sat_event_msg_recv_data msg_recv;
};

enum ctr_sat_pin {
	CTR_SAT_PIN_RESET = 0,
	CTR_SAT_PIN_WAKEUP = 1
};

typedef void (*ctr_sat_event_cb)(enum ctr_sat_event event, union ctr_sat_event_data *data,
				 void *user_data);

struct ctr_sat {

	atomic_t is_started;
	struct k_mutex mutex;

	uint8_t tx_buf[TX_MESSAGE_MAX_SIZE];
	size_t tx_buf_len;

	uint8_t rx_buf[RX_MESSAGE_MAX_SIZE];
	size_t rx_buf_len;

	ctr_sat_event_cb user_cb;
	void *user_data;

	uint16_t enqued_payload_ids[MAX_PAYLOADS];
	uint16_t enqued_payload_count;
	uint16_t last_payload_id;

	union {
		struct ctr_sat_syscon {
			struct k_poll_signal rx_completed_signal;

			uint8_t *tx_buf_transmit_ptr;
			uint8_t *rx_buf_receive_ptr;

			const struct device *uart_dev;
			struct gpio_dt_spec reset_gpio;
			struct gpio_dt_spec wakeup_gpio;
			struct gpio_dt_spec event_gpio;

			struct gpio_callback event_cb;
		} syscon;

		struct ctr_sat_w1 {
			uint64_t serial_number;
			const struct device *ds28e17_dev;

			uint8_t gpio_state;
		} w1;
	} hw_abstraction;

	int (*ctr_sat_uart_write_read)(struct ctr_sat *sat);
	int (*ctr_sat_gpio_write)(struct ctr_sat *sat, enum ctr_sat_pin pin, bool value);

	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_CTR_SAT_THREAD_STACK_SIZE);
	struct k_thread thread;
	k_tid_t thread_id;
	atomic_t thread_cancelation_requested;
	struct k_sem thread_cancelation_complete;
	struct k_sem event_trig_sem;
};

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_event_cb user_cb, void *user_data);
int ctr_sat_v1_init_syscon(struct ctr_sat *sat);
int ctr_sat_v1_init_w1(struct ctr_sat *sat);
int ctr_sat_start(struct ctr_sat *sat);
int ctr_sat_stop(struct ctr_sat *sat);
int ctr_sat_send_message(struct ctr_sat *sat, ctr_sat_msg_handle *msg_handle, const void *buf,
			 size_t len);
int ctr_sat_flush_messages(struct ctr_sat *sat);
int ctr_sat_use_wifi_devkit(struct ctr_sat *sat, const char *ssid, const char *password,
			    const char *api_key);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_SAT_H_ */
