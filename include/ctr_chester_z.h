#ifndef CTR_CHESTER_Z
#define CTR_CHESTER_Z

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_chester_z_buzzer_command {
	CTR_CHESTER_Z_BUZZER_COMMAND_NONE = 0,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_1_16 = 1,
	CTR_CHESTER_Z_BUZZER_COMMAND_4X_1_16 = 2,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_1_8 = 3,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_1_4 = 4,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_1_2 = 5,
	CTR_CHESTER_Z_BUZZER_COMMAND_2X_1_2 = 6,
	CTR_CHESTER_Z_BUZZER_COMMAND_3X_1_2 = 7,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_1_1 = 8,
	CTR_CHESTER_Z_BUZZER_COMMAND_1X_2_1 = 9,
};

enum ctr_chester_z_buzzer_pattern {
	CTR_CHESTER_Z_BUZZER_PATTERN_NONE = 0,
	CTR_CHESTER_Z_BUZZER_PATTERN_1_HZ_1_1 = 1,
	CTR_CHESTER_Z_BUZZER_PATTERN_1_HZ_1_7 = 2,
	CTR_CHESTER_Z_BUZZER_PATTERN_2_HZ_1_1 = 3,
	CTR_CHESTER_Z_BUZZER_PATTERN_2_HZ_1_3 = 4,
	CTR_CHESTER_Z_BUZZER_PATTERN_4_HZ_1_1 = 5,
	CTR_CHESTER_Z_BUZZER_PATTERN_8_HZ_1_1 = 6,
};

enum ctr_chester_z_led_channel {
	CTR_CHESTER_Z_LED_CHANNEL_0_R = 0,
	CTR_CHESTER_Z_LED_CHANNEL_0_G = 1,
	CTR_CHESTER_Z_LED_CHANNEL_0_B = 2,
	CTR_CHESTER_Z_LED_CHANNEL_1_R = 3,
	CTR_CHESTER_Z_LED_CHANNEL_1_G = 4,
	CTR_CHESTER_Z_LED_CHANNEL_1_B = 5,
	CTR_CHESTER_Z_LED_CHANNEL_2_R = 6,
	CTR_CHESTER_Z_LED_CHANNEL_2_G = 7,
	CTR_CHESTER_Z_LED_CHANNEL_2_B = 8,
	CTR_CHESTER_Z_LED_CHANNEL_3_R = 9,
	CTR_CHESTER_Z_LED_CHANNEL_3_G = 10,
	CTR_CHESTER_Z_LED_CHANNEL_3_B = 11,
	CTR_CHESTER_Z_LED_CHANNEL_4_R = 12,
	CTR_CHESTER_Z_LED_CHANNEL_4_G = 13,
	CTR_CHESTER_Z_LED_CHANNEL_4_B = 14,
};

enum ctr_chester_z_led_brightness {
	CTR_CHESTER_Z_LED_BRIGHTNESS_LOW = 0x1f,
	CTR_CHESTER_Z_LED_BRIGHTNESS_MEDIUM = 0x7f,
	CTR_CHESTER_Z_LED_BRIGHTNESS_HIGH = 0xff,
};

enum ctr_chester_z_led_command {
	CTR_CHESTER_Z_LED_COMMAND_NONE = 0,
	CTR_CHESTER_Z_LED_COMMAND_1X_1_16 = 1,
	CTR_CHESTER_Z_LED_COMMAND_4X_1_16 = 2,
	CTR_CHESTER_Z_LED_COMMAND_1X_1_8 = 3,
	CTR_CHESTER_Z_LED_COMMAND_1X_1_4 = 4,
	CTR_CHESTER_Z_LED_COMMAND_1X_1_2 = 5,
	CTR_CHESTER_Z_LED_COMMAND_2X_1_2 = 6,
	CTR_CHESTER_Z_LED_COMMAND_3X_1_2 = 7,
	CTR_CHESTER_Z_LED_COMMAND_1X_1_1 = 8,
	CTR_CHESTER_Z_LED_COMMAND_1X_2_1 = 9,
};

enum ctr_chester_z_led_pattern {
	CTR_CHESTER_Z_LED_PATTERN_NONE = 0,
	CTR_CHESTER_Z_LED_PATTERN_1_HZ_1_1 = 1,
	CTR_CHESTER_Z_LED_PATTERN_1_HZ_1_7 = 2,
	CTR_CHESTER_Z_LED_PATTERN_2_HZ_1_1 = 3,
	CTR_CHESTER_Z_LED_PATTERN_2_HZ_1_3 = 4,
	CTR_CHESTER_Z_LED_PATTERN_4_HZ_1_1 = 5,
	CTR_CHESTER_Z_LED_PATTERN_8_HZ_1_1 = 6,
};

enum ctr_chester_z_event {
	CTR_CHESTER_Z_EVENT_DC_CONNECTED = 0,
	CTR_CHESTER_Z_EVENT_DC_DISCONNECTED = 1,
	CTR_CHESTER_Z_EVENT_BUTTON_0_PRESS = 2,
	CTR_CHESTER_Z_EVENT_BUTTON_0_CLICK = 3,
	CTR_CHESTER_Z_EVENT_BUTTON_0_HOLD = 4,
	CTR_CHESTER_Z_EVENT_BUTTON_0_RELEASE = 5,
	CTR_CHESTER_Z_EVENT_BUTTON_1_PRESS = 6,
	CTR_CHESTER_Z_EVENT_BUTTON_1_CLICK = 7,
	CTR_CHESTER_Z_EVENT_BUTTON_1_HOLD = 8,
	CTR_CHESTER_Z_EVENT_BUTTON_1_RELEASE = 9,
	CTR_CHESTER_Z_EVENT_BUTTON_2_PRESS = 10,
	CTR_CHESTER_Z_EVENT_BUTTON_2_CLICK = 11,
	CTR_CHESTER_Z_EVENT_BUTTON_2_HOLD = 12,
	CTR_CHESTER_Z_EVENT_BUTTON_2_RELEASE = 13,
	CTR_CHESTER_Z_EVENT_BUTTON_3_PRESS = 14,
	CTR_CHESTER_Z_EVENT_BUTTON_3_CLICK = 15,
	CTR_CHESTER_Z_EVENT_BUTTON_3_HOLD = 16,
	CTR_CHESTER_Z_EVENT_BUTTON_3_RELEASE = 17,
	CTR_CHESTER_Z_EVENT_BUTTON_4_PRESS = 18,
	CTR_CHESTER_Z_EVENT_BUTTON_4_CLICK = 19,
	CTR_CHESTER_Z_EVENT_BUTTON_4_HOLD = 20,
	CTR_CHESTER_Z_EVENT_BUTTON_4_RELEASE = 21,
};

struct ctr_chester_z_buzzer_param {
	enum ctr_chester_z_buzzer_command command;
	enum ctr_chester_z_buzzer_pattern pattern;
};

struct ctr_chester_z_led_param {
	enum ctr_chester_z_led_brightness brightness;
	enum ctr_chester_z_led_command command;
	enum ctr_chester_z_led_pattern pattern;
};

struct ctr_chester_z_status {
	bool dc_input_connected;
	bool button_0_pressed;
	bool button_1_pressed;
	bool button_2_pressed;
	bool button_3_pressed;
	bool button_4_pressed;
};

typedef void (*ctr_chester_z_event_cb)(enum ctr_chester_z_event event, void *param);

int ctr_chester_z_init(ctr_chester_z_event_cb cb, void *param);
int ctr_chester_z_apply(void);
int ctr_chester_z_get_status(struct ctr_chester_z_status *status);
int ctr_chester_z_get_vdc_mv(uint16_t *vdc);
int ctr_chester_z_get_vbat_mv(uint16_t *vbat);
int ctr_chester_z_get_serial_number(uint32_t *serial_number);
int ctr_chester_z_get_hw_revision(uint16_t *hw_revision);
int ctr_chester_z_get_hw_variant(uint32_t *hw_variant);
int ctr_chester_z_get_fw_version(uint32_t *fw_version);
int ctr_chester_z_get_vendor_name(char **vendor_name);
int ctr_chester_z_get_product_name(char **product_name);
int ctr_chester_z_set_buzzer(const struct ctr_chester_z_buzzer_param *param);
int ctr_chester_z_set_led(enum ctr_chester_z_led_channel channel,
                          const struct ctr_chester_z_led_param *param);

#ifdef __cplusplus
}
#endif

#endif
