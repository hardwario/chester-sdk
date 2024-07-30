/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */
/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/drivers/led.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(ctr_led, LOG_LEVEL_INF);

const struct device *ctr_led_mainboard = DEVICE_DT_GET(DT_NODELABEL(gpio_leds));

struct seq_wrap {
	struct ctr_led_seq seq;
	struct k_timer *timer;
	int32_t idx;
	bool is_highest_prio;
};

struct timer_wrap {
	struct seq_wrap *owner;
	struct k_timer timer;
};

struct led {
	const struct device *dev;
	struct k_mutex lock;
	struct seq_wrap seqs[8];
	struct timer_wrap timers[8];
	size_t seq_count;
	struct k_timer fade_timer;
};

/* clang-format off */
static struct led m_leds[] = {
#define F_BODY(node, prop, idx)                                                                           \
	{                                                                                          \
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx))                                              \
	},

DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), ctr_leds, F_BODY)

#undef F_BODY
};
/* clang-format on */

#define BYTE(n, b) (((n) >> ((b) * 8)) & 0xff)

static struct k_timer *alloc_timer(struct led *led, struct seq_wrap *s)
{
	for (int i = 0; i < ARRAY_SIZE(led->timers); i++) {
		if (led->timers[i].owner == NULL) {
			led->timers[i].owner = s;
			return &led->timers[i].timer;
		}
	}

	return NULL;
}

static void handle_led(struct led *led, struct seq_wrap *s)
{
	const struct ctr_led_cmd *cmd = &s->seq.cmds[s->idx];
	if (cmd->fade) {
		const struct ctr_led_cmd *next_cmd =
			&s->seq.cmds[s->idx >= s->seq.cmds_len - 1 ? 0 : s->idx + 1];
		int64_t remaining_ticks = k_timer_remaining_ticks(s->timer);
		const uint64_t src = cmd->value;
		const uint64_t tgt = next_cmd->value;
		const int64_t length = k_ms_to_ticks_floor64(cmd->length);

		for (int i = 0; i < sizeof(ctr_led_value); i++) {
			const int32_t diff = (int32_t)BYTE(tgt, i) - BYTE(src, i);
			const uint8_t color =
				BYTE(src, i) + diff * (length - remaining_ticks) / length;
			if (led_set_brightness(led->dev, i, color * 100 / 255)) {
				break;
			}
		}

		k_timer_start(&led->fade_timer, K_MSEC(4), K_FOREVER);
	} else {
		const uint64_t val = cmd->value;
		for (int i = 0; i < sizeof(ctr_led_value); i++) {
			if (led_set_brightness(led->dev, i, BYTE(val, i) * 100 / 255)) {
				break;
			}
		}
	}
}

static struct seq_wrap *find_highest_prio(struct led *led)
{
	struct seq_wrap *s = NULL;
	for (int i = 0; i < led->seq_count; i++) {
		led->seqs[i].is_highest_prio = false;
		if (s == NULL || led->seqs[i].seq.prio > s->seq.prio) {
			s = &led->seqs[i];
		}
	}

	if (s != NULL) {
		s->is_highest_prio = true;
	}

	return s;
}

static int remove_seq(struct led *led, struct seq_wrap *s)
{
	int index = s - led->seqs;
	if (index >= led->seq_count) {
		return -EINVAL;
	}

	CONTAINER_OF(s->timer, struct timer_wrap, timer)->owner = NULL;

	for (int i = index + 1; i < led->seq_count; i++) {
		CONTAINER_OF(led->seqs[i].timer, struct timer_wrap, timer)->owner--;
		led->seqs[i - 1] = led->seqs[i];
	}

	led->seq_count--;

	s = find_highest_prio(led);
	if (s != NULL) {
		handle_led(led, s);
	}

	return 0;
}

static void seq_timer_expire(struct k_timer *tim)
{
	struct led *led = k_timer_user_data_get(tim);
	if (k_mutex_lock(&led->lock, K_NO_WAIT) < 0) {
		k_timer_start(tim, K_TICKS(10), K_FOREVER);
		return;
	}

	struct timer_wrap *tw = CONTAINER_OF(tim, struct timer_wrap, timer);
	struct seq_wrap *s = tw->owner;

	s->idx++;
	if (s->idx >= s->seq.cmds_len) {
		if (s->seq.loop) {
			s->idx = 0;
		} else {
			if (s->is_highest_prio) {
				k_timer_stop(&led->fade_timer);
			}

			remove_seq(led, s);
			k_mutex_unlock(&led->lock);
			return;
		}
	}

	k_timer_start(tim, K_MSEC(s->seq.cmds[s->idx].length), K_FOREVER);

	if (s->is_highest_prio) {
		handle_led(led, s);
	}

	k_mutex_unlock(&led->lock);
}

static void fade_timer_expire(struct k_timer *tim)
{
	struct led *led = CONTAINER_OF(tim, struct led, fade_timer);
	if (k_mutex_lock(&led->lock, K_NO_WAIT) < 0) {
		k_timer_start(tim, K_TICKS(10), K_FOREVER);
		return;
	}

	/* This could be done better */
	struct seq_wrap *s = NULL;
	for (int i = 0; i < ARRAY_SIZE(led->seqs); i++) {
		if (led->seqs[i].is_highest_prio) {
			s = &led->seqs[i];
			break;
		}
	}

	if (s == NULL) {
		k_mutex_unlock(&led->lock);
		return;
	}

	handle_led(led, s);
	k_mutex_unlock(&led->lock);
}

int ctr_led_play(const struct device *dev, struct ctr_led_seq seq)
{
	struct led *led = NULL;
	for (int i = 0; i < ARRAY_SIZE(m_leds); i++) {
		if (dev == m_leds[i].dev) {
			led = &m_leds[i];
		}
	}

	if (led == NULL) {
		return -ENODEV;
	}

	if (led->seq_count == 8) {
		return -ENOMEM;
	}

	struct seq_wrap s = (struct seq_wrap){
		.seq = seq,
		.idx = -1,
	};

	k_mutex_lock(&led->lock, K_FOREVER);
	struct seq_wrap *const sp = &led->seqs[led->seq_count];
	*sp = s;
	led->seq_count++;
	sp->timer = alloc_timer(led, sp);
	find_highest_prio(led);

	seq_timer_expire(sp->timer);
	k_mutex_unlock(&led->lock);

	return 0;
}

int ctr_led_stop(const struct device *dev, struct ctr_led_seq seq)
{
	struct led *led = NULL;
	for (int i = 0; i < ARRAY_SIZE(m_leds); i++) {
		if (dev == m_leds[i].dev) {
			led = &m_leds[i];
		}
	}

	if (led == NULL) {
		return -ENODEV;
	}

	k_mutex_lock(&led->lock, K_FOREVER);

	for (int i = 0; i < led->seq_count; i++) {
		if (led->seqs[i].seq.cmds == seq.cmds) {
			struct seq_wrap *s = &led->seqs[i];
			k_timer_stop(s->timer);
			int ret = remove_seq(led, s);
			if (ret) {
				return ret;
			}
		}
	}
	k_mutex_unlock(&led->lock);
	return 0;
}

int ctr_led_blink(const struct device *dev, uint32_t color, k_timeout_t length)
{
	struct ctr_led_cmd cmds[2] = {
		{
			.length = k_ticks_to_ms_floor64(length.ticks),
			.value = color,
		},
		{
			.length = 0,
			.value = 0,
		},
	};

	struct ctr_led_seq seq = {
		.cmds = cmds,
		.cmds_len = 2,
		.loop = false,
		.prio = CTR_LED_PRIO_HIGH,
	};

	int ret = ctr_led_play(dev, seq);
	if (ret) {
		LOG_ERR("Call `ctr_led_play` failed: %d", ret);
		return ret;
	}
	k_sleep(length);

	return 0;
}

int ctr_led_fade(const struct device *dev, uint32_t from, uint32_t to, k_timeout_t length)
{
	struct ctr_led_cmd cmds[2] = {
		{
			.length = k_ticks_to_ms_floor64(length.ticks),
			.value = from,
			.fade = true,
		},
		{
			.length = 0,
			.value = to,
			.fade = false,
		},
	};

	struct ctr_led_seq seq = {
		.cmds = cmds,
		.cmds_len = ARRAY_SIZE(cmds),
		.loop = false,
		.prio = CTR_LED_PRIO_HIGH,
	};

	int ret = ctr_led_play(dev, seq);
	if (ret) {
		LOG_ERR("Call `ctr_led_play` failed: %d", ret);
		return ret;
	}
	k_sleep(length);

	return 0;
}

static int init(void)
{
	LOG_INF("System Initialization");

	for (int i = 0; i < ARRAY_SIZE(m_leds); i++) {
		k_timer_init(&m_leds[i].fade_timer, fade_timer_expire, NULL);
		k_timer_user_data_set(&m_leds[i].fade_timer, NULL);
		for (int j = 0; j < ARRAY_SIZE(m_leds[i].timers); j++) {
			k_timer_init(&m_leds[i].timers[j].timer, seq_timer_expire, NULL);
			k_timer_user_data_set(&m_leds[i].timers[j].timer, &m_leds[i]);
		}
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_LED_INIT_PRIORITY);