/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_LED_H_
#define CHESTER_INCLUDE_CTR_LED_H_

#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ctr_led
 * @brief Subsystem for sequencing LED commands
 * @{
 */

/**
 * @brief Shortcut to the CHESTER mainboard LED + the EXT LED
 */
extern const struct device *ctr_led_mainboard;

/**
 * @brief Value for an LED
 *
 * Each consecutive byte corresponds to one channel of the underlying LED, starting with the least
 * significant byte. If an LED doesn't have brigtness functionality, 255 is still used as the value
 * for 'ON'.
 */
typedef uint64_t ctr_led_value;

/**
 * @brief Channels of the mainboard LED
 */
enum ctr_led_channel {
	CTR_LED_CHANNEL_R = 0x0000ff,
	CTR_LED_CHANNEL_G = 0x00ff00,
	CTR_LED_CHANNEL_Y = 0xff0000,
	CTR_LED_CHANNEL_EXT = 0xff000000,
	CTR_LED_CHANNEL_LOAD = 0xff00000000,
};

/**
 * @brief LED Command
 */
struct ctr_led_cmd {
	ctr_led_value value; /**< Value to set the LED to during the runtime of the command. */
	int64_t length;      /**< Length of the command in ms. */
	bool fade;           /**< If true, the command's value will be faded into the next one. */
};

/**
 * @brief Sequence priority
 */
enum ctr_led_prio {
	CTR_LED_PRIO_LOW,
	CTR_LED_PRIO_MEDIUM,
	CTR_LED_PRIO_HIGH
};

/**
 * @brief LED command sequence
 */
struct ctr_led_seq {
	const struct ctr_led_cmd *cmds; /**< Array of the commands. This array has to live as long
					   as the sequence is played. */
	size_t cmds_len;                /**< Number of commands in the sequence. */
	enum ctr_led_prio prio;         /**< Priority of the sequence. */
	bool loop; /**< If true, the sequence will start again after finishing, instead of being
		      deleted. */
};

/**
 * @brief Convenience macro for creating static sequences.
 */
#define CTR_LED_SEQ_DEFINE(name, prio_arg, loop_arg, ...)                                          \
	static const struct ctr_led_cmd name##__cmds[] = {__VA_ARGS__};                            \
	static const struct ctr_led_seq name = {                                                   \
		.cmds = name##__cmds,                                                              \
		.cmds_len = ARRAY_SIZE(name##__cmds),                                              \
		.prio = prio_arg,                                                                  \
		.loop = loop_arg,                                                                  \
	}

/**
 * @brief Old API for controlling the mainboard LED. Do not use with the new API.
 */
static inline int ctr_led_set(enum ctr_led_channel channel, bool is_on)
{
	switch (channel) {
	case CTR_LED_CHANNEL_R:
		channel = 0;
		break;
	case CTR_LED_CHANNEL_G:
		channel = 1;
		break;
	case CTR_LED_CHANNEL_Y:
		channel = 2;
		break;
	case CTR_LED_CHANNEL_EXT:
		channel = 3;
		break;
	case CTR_LED_CHANNEL_LOAD:
		channel = 4;
		break;
	}

	if (is_on) {
		return led_on(DEVICE_DT_GET(DT_NODELABEL(gpio_leds)), channel);
	} else {
		return led_off(DEVICE_DT_GET(DT_NODELABEL(gpio_leds)), channel);
	}
}

/**
 * @brief Play a sequence.
 * @param[in] dev
 * @param[in] seq
 * @retval 0 on success
 * @retval negative on error
 */
int ctr_led_play(const struct device *dev, struct ctr_led_seq seq);

/**
 * @brief Stop a sequence.
 * @param[in] dev
 * @param[in] seq
 * @retval 0 on success
 * @retval negative on error
 */
int ctr_led_stop(const struct device *dev, struct ctr_led_seq seq);

/**
 * @brief Set the LED to a color for the specified amount of time. This function call is blocking.
 * @param[in] dev
 * @param[in] color
 * @param[in] time
 * @retval 0 on success
 * @retval negative on error
 */
int ctr_led_blink(const struct device *dev, uint32_t color, k_timeout_t time);

/**
 * @brief Fade the LED from one value to another. This function call is blocking.
 * @param[in] dev
 * @param[in] from
 * @param[in] to
 * @param[in] time
 * @retval 0 on success
 * @retval negative on error
 */
int ctr_led_fade(const struct device *dev, uint32_t from, uint32_t to, k_timeout_t length);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_LED_H_ */
