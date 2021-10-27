#ifndef CHESTER_INCLUDE_LTE_TALK_H_
#define CHESTER_INCLUDE_LTE_TALK_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_lte_talk_event {
	CTR_LTE_TALK_EVENT_BOOT = 0,
};

typedef void (*ctr_lte_talk_event_cb)(enum ctr_lte_talk_event event);

int ctr_lte_talk_init(ctr_lte_talk_event_cb event_cb);
int ctr_lte_talk_enable(void);
int ctr_lte_talk_disable(void);
int ctr_lte_talk_at(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_LTE_TALK_H_ */
