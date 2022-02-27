#ifndef CHESTER_INCLUDE_CTR_WDOG_H_
#define CHESTER_INCLUDE_CTR_WDOG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_wdog_channel {
	int id;
};

int ctr_wdog_set_timeout(int timeout_msec);
int ctr_wdog_install(struct ctr_wdog_channel *channel);
int ctr_wdog_start(void);
int ctr_wdog_feed(struct ctr_wdog_channel *channel);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_WDOG_H_ */
