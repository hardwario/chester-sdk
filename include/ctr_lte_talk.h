#ifndef CHESTER_INCLUDE_LTE_TALK_H_
#define CHESTER_INCLUDE_LTE_TALK_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hio_lte_talk_event {
	HIO_LTE_TALK_EVENT_BOOT = 0,
};

typedef void (*hio_lte_talk_event_cb)(enum hio_lte_talk_event event);

int hio_lte_talk_init(hio_lte_talk_event_cb event_cb);
int hio_lte_talk_enable(void);
int hio_lte_talk_disable(void);
int hio_lte_talk_at(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LTE_TALK_H_ */
