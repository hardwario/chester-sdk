/* CHESTER includes */
#include "astronode_s.h"
#include "ctr_sat_v1_internal.h"

#include <chester/ctr_sat.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_sat_v1, CONFIG_CTR_SAT_LOG_LEVEL);

static int ctr_sat_get_pending_event(struct ctr_sat *sat, int *event)
{
	int ret;

	LOG_DBG("Getting pending events");

	uint8_t response;
	size_t response_size = sizeof(response);
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_EVT_R, NULL, 0, &response,
				      &response_size, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	*event = (int)response;

	return 0;
}

static int ctr_sat_handle_ack(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Reading satelite ack");

	uint16_t payload_id;
	size_t payload_id_size = sizeof(payload_id);

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_SAK_R, NULL, 0, &payload_id,
				      &payload_id_size, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` (1) failed: %d", ret);
		return ret;
	}

	if (sat->user_cb) {
		union ctr_sat_event_data data = {
			.msg_send = {.msg = (ctr_sat_msg_handle)payload_id}};
		sat->user_cb(CTR_SAT_EVENT_MESSAGE_SENT, &data, sat->user_data);
	}

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_SAK_C, NULL, 0, NULL, NULL,
				      FLAG_REPEAT_ON_REQ_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` (2) failed: %d", ret);
		return ret;
	}

	LOG_DBG("Sateilite message 0x%04x was processed", payload_id);

	return 0;
}

static int ctr_sat_clear_reset_event(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Clearing reset event");

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_RES_C, NULL, 0, NULL, NULL,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Reset event cleared");

	return 0;
}

static int ctr_sat_reconfigure(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Reading module configuration");

	uint8_t cur_cfg[ASTRONODE_S_CFG_RA_SIZE];
	size_t cur_cfg_size = sizeof(cur_cfg);
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_R, NULL, 0, cur_cfg, &cur_cfg_size,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	LOG_INF("=========== Atronode S configuration ===========");
	if (cur_cfg[ASTRONODE_S_CFG_RA_PRODUCT_ID] == 3) {
		LOG_INF("Product ID: Commercial Satellite Astronode");
	} else if (cur_cfg[ASTRONODE_S_CFG_RA_PRODUCT_ID] == 4) {
		LOG_INF("Product ID: Commercial Wi-Fi Dev Kit");
	} else {
		LOG_INF("Product ID: Unknown");
	}
	LOG_INF("Hardware revision: %d", cur_cfg[ASTRONODE_S_CFG_RA_HW_REV]);
	LOG_INF("Firmware version: %d.%d.%d", cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_MAJOR],
		cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_MINOR],
		cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_REVISION]);
	LOG_INF("");

	LOG_INF("Payload Acknowledgement: %s",
		FIELD_GET(ASTRONODE_S_CFG1_ACK_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Add Geolocation: %s",
		FIELD_GET(ASTRONODE_S_CFG1_ADD_GEO_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Ephemeris Enabled: %s",
		FIELD_GET(ASTRONODE_S_CFG1_EPHEMERIS_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Deep Sleep Mode: %s",
		FIELD_GET(ASTRONODE_S_CFG1_DEEPSLEEP_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("");

	LOG_INF("Payload Ack Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_ACK_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Reset Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_RST_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Command Avalaible Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_CMD_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Message TX Pending Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_TX_PENDING_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("================================================");

	uint8_t new_cfg[ASTRONODE_S_CFG_WR_SIZE];

	new_cfg[ASTRONODE_S_CFG_WR_CFG1] = FIELD_PREP(ASTRONODE_S_CFG1_ACK_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG1_ADD_GEO_MASK, 0) |
					   FIELD_PREP(ASTRONODE_S_CFG1_EPHEMERIS_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG1_DEEPSLEEP_EN_MASK, 0);

	new_cfg[ASTRONODE_S_CFG_WR_CFG2] = 0;

	new_cfg[ASTRONODE_S_CFG_WR_CFG3] = FIELD_PREP(ASTRONODE_S_CFG3_INT_ACK_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_RST_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_CMD_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_TX_PENDING_EN_MASK, 0);

	LOG_DBG("Updating module configuration");

	if (memcpy(cur_cfg + ASTRONODE_S_CFG_RA_CFG1, new_cfg, sizeof(new_cfg)) != 0) {
		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_W, new_cfg, sizeof(new_cfg),
					      NULL, NULL, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
		if (ret) {
			LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
			return ret;
		}

		LOG_INF("Configuration was adjusted");
	}

	return 0;
}

#if CONFIG_CTR_SAT_RESET_ON_INIT
static int ctr_sat_reset(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Resseting module");

	ret = sat->ctr_sat_gpio_write(sat, CTR_SAT_PIN_RESET, true);
	if (ret) {
		LOG_ERR("Call `ctr_sat_gpio_write` (1) failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = sat->ctr_sat_gpio_write(sat, CTR_SAT_PIN_RESET, false);
	if (ret) {
		LOG_ERR("Call `ctr_sat_gpio_write` (2) failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(ASTRONODE_MAX_RESET_WAKEUP_TIME_MSEC + 1));

	return 0;
}
#endif

static int ctr_sat_receive_command(struct ctr_sat *sat)
{
	int ret;

	/* last byte is used for terminating possible string */
	uint8_t cmd_ra[ASTRONODE_S_CMD_RA_SIZE + 1];
	size_t cmd_ra_size = sizeof(cmd_ra) - 1;

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_R, NULL, 0, cmd_ra, &cmd_ra_size,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS | FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	cmd_ra[cmd_ra_size] = '\0';

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_C, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	if (sat->user_cb) {
		/* Astrnode date is defined as number of seconds since 2018-01-01 00:00:00 which is
		 * equivalent to unix timestamp 1514764800.*/
		time_t astronode_time_offset = 1514764800UL;

		union ctr_sat_event_data data = {
			.msg_recv = {
				.data = cmd_ra + ASTRONODE_S_CMD_RA_PAYLOAD,
				.data_len = cmd_ra_size - ASTRONODE_S_CMD_RA_PAYLOAD,
				.created_at =
					(time_t)(astronode_time_offset +
						 sys_get_le32(cmd_ra +
							      ASTRONODE_S_CMD_RA_CREATED_DATE))}};
		sat->user_cb(CTR_SAT_EVENT_MESSAGE_RECEIVED, &data, sat->user_data);
	}

	return 0;
}

static int ctr_sat_handle_event(struct ctr_sat *sat)
{
	int ret;

	int event;
	ret = ctr_sat_get_pending_event(sat, &event);
	if (ret) {
		LOG_DBG("Call `ctr_sat_get_pending_event` failed: %d", ret);
		return ret;
	}

	if (event == 0) {
		LOG_DBG("Spurious interrupt detected. Astrnonode module has no event flag set");
		return -EAGAIN;
	}

	LOG_DBG("Astronode event 0x%02x", event);

	if (event & ASTRONODE_S_EVENT_MODULE_RESET) {
		ret = ctr_sat_clear_reset_event(sat);
		if (ret) {
			LOG_WRN("Call `ctr_sat_clear_reset_event` failed: %d", ret);
		} else {
			ret = ctr_sat_reconfigure(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_reconfigure` failed: %d", ret);
			}
		}
	}

	if (event & ASTRONODE_S_EVENT_SAT_ACK) {
		ret = ctr_sat_handle_ack(sat);
		if (ret) {
			LOG_ERR("Call `ctr_sat_handle_ack` failed: %d", ret);
		}
	}

	if (event & ASTRONODE_S_EVENT_COMMAND_AVAILABLE) {
		ret = ctr_sat_receive_command(sat);
		if (ret) {
			LOG_ERR("Call `ctr_sat_receive_command` failed: %d", ret);
		}
	}

	return 0;
}

static int ctr_sat_poll_interrupt(struct ctr_sat *sat)
{
	/*
	int ret;

	ret = gpio_pin_get_dt(&sat->event_gpio);
	if (ret != 0 && ret != 1) {
		LOG_ERR("Call `gpio_pin_get_dt` failed: %d", ret);
		return ret;
	}

	if (ret == 1) {
		return ctr_sat_handle_event(sat);
	} else {
		return 0;
	}
	*/

	return 0;
}

static void ctr_sat_thread_main(struct ctr_sat *sat)
{
	int ret;

	/*
		Thread is responsible for two tasks:
			- Deferred processing of events (blocking tasks triggered by interrupt)
			- Polling interrupt (needed when using over W1)
	*/

	LOG_INF("CTR_SAT thread started");

	while (true) {
		if (atomic_get(&sat->thread_cancelation_requested) == 1) {
			k_sem_give(&sat->thread_cancelation_complete);
			return;
		}

		ret = k_sem_take(&sat->event_trig_sem, K_MSEC(CONFIG_CTR_SAT_INT_POLL_PERIOD_MSEC));
		if (ret == 0) {
			ret = ctr_sat_handle_event(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_handle_event` failed: %d", ret);
			}
		} else if (ret == -EAGAIN) {
			ret = ctr_sat_poll_interrupt(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_poll_interrupt` failed: %d", ret);
			}
		} else {
			LOG_WRN("Call `k_sem_take` failed: %d", ret);
			k_sleep(K_MSEC(1));
		}
	}
}

int ctr_sat_init_generic(struct ctr_sat *sat)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	sat->last_payload_id = 0;
	sat->enqued_payload_count = 0;

	sat->user_cb = NULL;
	sat->user_data = NULL;

	k_mutex_init(&sat->mutex);
	k_sem_init(&sat->event_trig_sem, 0, K_SEM_MAX_LIMIT);

#if CONFIG_CTR_SAT_RESET_ON_INIT
	ret = ctr_sat_reset(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_reset` failed: %d", ret);
		return ret;
	}
#endif

	ret = ctr_sat_reconfigure(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_reconfigure` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_sat_start(struct ctr_sat *sat)
{
	sat->thread_id =
		k_thread_create(&sat->thread, sat->thread_stack, CONFIG_CTR_SAT_THREAD_STACK_SIZE,
				(k_thread_entry_t)ctr_sat_thread_main, sat, NULL, NULL,
				K_PRIO_COOP(CONFIG_CTR_SAT_THREAD_PRIORITY), 0, K_NO_WAIT);

	return 0;
}

int ctr_sat_stop(struct ctr_sat *sat)
{
	atomic_set(&sat->thread_cancelation_requested, 1);
	k_sem_take(&sat->thread_cancelation_complete, K_FOREVER);
	return 0;
}

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_event_cb user_cb, void *user_data)
{
	sat->user_cb = user_cb;
	sat->user_data = user_data;

	return 0;
}

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
int ctr_sat_use_wifi_devkit(struct ctr_sat *sat, const char *ssid, const char *password,
			    const char *api_key)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	ssize_t ssid_len = strlen(ssid);
	if (ssid_len < 1 || ssid_len > (ASTRONODE_S_WFI_WR_SSID_SIZE - 1)) {
		LOG_ERR("Wi-Fi SSID length exceeded maximum allowed length %d",
			ASTRONODE_S_WFI_WR_SSID_SIZE - 1);
		return -EINVAL;
	}

	ssize_t password_len = strlen(password);
	if (password_len < 1 || password_len > (ASTRONODE_S_WFI_WR_PASSWORD_SIZE - 1)) {
		LOG_ERR("Wi-Fi password length exceeded maxmimum allowed length %d",
			ASTRONODE_S_WFI_WR_PASSWORD_SIZE - 1);
		return -EINVAL;
	}

	ssize_t api_key_len = strlen(api_key);
	if (api_key_len != (ASTRONODE_S_WFI_WR_API_KEY_SIZE - 1)) {
		LOG_ERR("API key must be exactly 96 characters. You passed string of length %d",
			api_key_len);
		return -EINVAL;
	}

	uint8_t req[ASTRONODE_S_WFI_WR_SIZE] = {0};
	memcpy(req + ASTRONODE_S_WFI_WR_SSID, ssid, ssid_len);
	memcpy(req + ASTRONODE_S_WFI_WR_PASSWORD, password, password_len);
	memcpy(req + ASTRONODE_S_WFI_WR_API_KEY, api_key, api_key_len);

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_WIF_W, req, sizeof(req), NULL, NULL,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	}

	return 0;
}
#endif

static uint16_t ctr_sat_get_next_payload_id(struct ctr_sat *sat)
{
	uint16_t next_id = sat->last_payload_id + 1;
	if (next_id == 0xFFFF) {
		next_id = 0;
	}

	sat->last_payload_id = next_id;

	return next_id;
}

int ctr_sat_send_message(struct ctr_sat *sat, ctr_sat_msg_handle *msg_handle, const void *buf,
			 const size_t len)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	if (sat->enqued_payload_count == MAX_PAYLOADS) {
		LOG_ERR("Unable to send message because end buffer is full");
		return -EAGAIN;
	}

	if (atomic_get(&sat->is_started) == 0) {
		LOG_ERR("satelite module is not started. Please call ctr_sat_start() "
			"first");
		ret = -EACCES;
		goto exit;
	}

	if (len > ASTRONODE_S_PLD_ER_DATA_SIZE) {
		LOG_DBG("Message is too long. Maximum allowed length is %d",
			ASTRONODE_S_PLD_ER_DATA_SIZE);
		ret = -EINVAL;
		goto exit;
	}

	uint16_t payload_id = ctr_sat_get_next_payload_id(sat);

	uint8_t req[ASTRONODE_S_PLD_ER_SIZE] = {0};
	sys_put_le16(payload_id, req + ASTRONODE_S_PLD_ER_ID);
	memcpy(req + ASTRONODE_S_PLD_ER_DATA, buf, len);

	bool was_previous_answer_affected_by_crc_error = false;
	do {
		uint8_t ans[ASTRONODE_S_PLD_EA_SIZE];
		size_t ans_size = sizeof(ans);

		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_E, req, 2 + len, &ans,
					      &ans_size, 0);
		if (ret) {
			if (ret == ASTRONODE_S_ERROR_DUPLICATE_ID) {
				if (was_previous_answer_affected_by_crc_error) {
					/* dumplicate ID here means that previous request was
					 * successfull and only answer was malformed */
					ret = 0;
					goto exit;
				} else {
					was_previous_answer_affected_by_crc_error = false;
					LOG_INF("Message with id 0x%04x was already enqueud. "
						"Retrying with different ID",
						payload_id);
					payload_id = ctr_sat_get_next_payload_id(sat);
					sys_put_le16(payload_id, req + ASTRONODE_S_PLD_ER_ID);
				}
			} else if (ret == ASTRONODE_S_ERROR_CRC_NOT_VALID) {
				was_previous_answer_affected_by_crc_error = false;
				LOG_INF("Enquing message with id 0x%04x failed because of CRC "
					"error on request transmission. Retrying with the same "
					"ID",
					payload_id);
			} else if (ret == -E_INVCRC) {
				was_previous_answer_affected_by_crc_error = true;
				LOG_INF("Enquing message with id 0x%04x failed because of CRC "
					"error on answer transmission. Retrying with the same ID",
					payload_id);
			} else {
				was_previous_answer_affected_by_crc_error = false;
				LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
				goto exit;
			}
		}
	} while (ret == ASTRONODE_S_ERROR_DUPLICATE_ID || ret == ASTRONODE_S_ERROR_CRC_NOT_VALID);

	if (msg_handle) {
		*msg_handle = payload_id;
	}

exit:
	return ret;
}

int ctr_sat_flush_messages(struct ctr_sat *sat)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	LOG_DBG("Flushing all messages");

	/* ASTRONODE_S_ERROR_BUFFER_EMPTY is allowed error */
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_F, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS | FLAG_NO_WARN_ASTRONODE_ERROR |
					      FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret) {
		if (ret < 0) {
			LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
			return ret;
		} else if (ret != ASTRONODE_S_ERROR_BUFFER_EMPTY) {
			LOG_WRN("Satellite module error 0x%04x", ret);
			return -EIO;
		}
	}

	return 0;
}
