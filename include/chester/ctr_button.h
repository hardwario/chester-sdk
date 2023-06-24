/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_BUTTON_H_
#define CHESTER_INCLUDE_CTR_BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_button_channel {
	CTR_BUTTON_CHANNEL_INT = 0,
	CTR_BUTTON_CHANNEL_EXT = 1,
};

enum ctr_button_event {
	CTR_BUTTON_EVENT_CLICK,
	CTR_BUTTON_EVENT_HOLD
};

typedef void (*ctr_button_event_cb)(enum ctr_button_channel, enum ctr_button_event event, int value,
	void *user_data);

int ctr_button_set_event_cb(ctr_button_event_cb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_BUTTON_H_ */
