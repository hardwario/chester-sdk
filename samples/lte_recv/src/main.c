/* CHESTER includes */
#include <ctr_buf.h>
#include <ctr_info.h>
#include <ctr_led.h>
#include <ctr_lte.h>

/* Zephyr includes */
#include <logging/log.h>
#include <sys/byteorder.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define KEY 0x3e2ce2d6e92bcfd3
#define TLV_OFFSET 22

static void process_response(const void *buf, size_t len)
{
	int ret;

	LOG_HEXDUMP_DBG(buf, len, "Read data");

	const uint8_t *p = buf;

	if (len < TLV_OFFSET) {
		LOG_ERR("Packet is less than %u bytes", TLV_OFFSET);
		return;
	}

	if (len != sys_get_le16(&p[0])) {
		LOG_ERR("Length mismatch");
		return;
	}

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return;
	}

	ret = tc_sha256_update(&s, &p[8], len - 8);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_update` failed: %d", ret);
		return;
	}

	uint8_t digest[32];
	ret = tc_sha256_final(digest, &s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_final` failed: %d", ret);
		return;
	}

	if (memcmp(digest, &p[2], 6)) {
		LOG_ERR("Hash mismatch");
		return;
	}

	if (KEY != sys_get_le64(&p[8])) {
		LOG_ERR("Key mismatch");
		return;
	}

	uint32_t serial_number;
	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return;
	}

	if (serial_number != sys_get_le32(&p[16])) {
		LOG_ERR("Serial number mismatch");
		return;
	}

	LOG_INF("Sequence number: %u", sys_get_le16(&p[20]));

	size_t tlv_len = len - TLV_OFFSET;
	p += TLV_OFFSET;

	while (tlv_len) {
		if (tlv_len < 2) {
			LOG_WRN("TLV frame corrupted");
			return;
		}

		uint8_t type = *p++;
		uint8_t length = *p++;
		const uint8_t *value = p;
		p += length;

		switch (type) {
		case 0x01:
			LOG_INF("Command: Set LED");

			if (length != 1) {
				LOG_WRN("Incorrent length: %u", length);
				return;
			}

			if (value[0] == 0) {
				LOG_INF("LED off");
				ctr_led_set(CTR_LED_CHANNEL_G, false);

			} else if (value[0] == 1) {
				LOG_INF("LED on");
				ctr_led_set(CTR_LED_CHANNEL_G, true);

			} else {
				LOG_WRN("Incorrent value: %u", value[0]);
				return;
			}

			break;

		default:
			LOG_WRN("Unknown type: %u", type);
			return;
		}

		tlv_len -= 2 + length;
	}
}

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

static void lte_event_handler(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
{
	switch (event) {
	case CTR_LTE_EVENT_FAILURE:
		LOG_ERR("Event `CTR_LTE_EVENT_FAILURE`");
		start();
		break;

	case CTR_LTE_EVENT_START_OK:
		LOG_INF("Event `CTR_LTE_EVENT_START_OK`");
		attach();
		break;

	case CTR_LTE_EVENT_START_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_START_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_ATTACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_ATTACH_OK`");
		break;

	case CTR_LTE_EVENT_ATTACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_ATTACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_DETACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_DETACH_OK`");
		break;

	case CTR_LTE_EVENT_DETACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_DETACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_EVAL_OK:
		LOG_INF("Event `CTR_LTE_EVENT_EVAL_OK`");
		break;

	case CTR_LTE_EVENT_EVAL_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_EVAL_ERR`");
		break;

	case CTR_LTE_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LTE_EVENT_SEND_OK`");
		if (data->send_ok.response) {
			process_response(data->send_ok.response, data->send_ok.response_len);
		}
		break;

	case CTR_LTE_EVENT_SEND_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_SEND_ERR`");
		start();
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
		return;
	}
}

static int compose(struct ctr_buf *buf)
{
	int ret;

	ctr_buf_reset(buf);

	ret = ctr_buf_seek(buf, 8);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u64(buf, KEY);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u64` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u32(buf, serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u32` failed: %d", ret);
		return ret;
	}

	static uint16_t sequence;
	ret = ctr_buf_append_u16(buf, sequence++);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16` failed: %d", ret);
		return ret;
	}

	size_t len = ctr_buf_get_used(buf);

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return ret;
	}

	ret = tc_sha256_update(&s, ctr_buf_get_mem(buf) + 8, len - 8);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_update` failed: %d", ret);
		return ret;
	}

	uint8_t digest[32];
	ret = tc_sha256_final(digest, &s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_final` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, 0);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u16(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, digest, 6);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int send(void)
{
	int ret;

	CTR_BUF_DEFINE(buf, 128);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	static uint8_t response[128];
	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 5999;
	opts.response = response;
	opts.response_size = sizeof(response);
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

void main(void)
{
	int ret;

	ret = ctr_lte_set_event_cb(lte_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = send();
		if (ret) {
			LOG_ERR("Call `send` failed: %d", ret);
		}

		k_sleep(K_SECONDS(5));
	}
}
