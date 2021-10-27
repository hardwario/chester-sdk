#ifndef CHESTER_INCLUDE_THERM_H_
#define CHESTER_INCLUDE_THERM_H_

#ifdef __cplusplus
extern "C" {
#endif

int ctr_therm_init(void);
int ctr_therm_read(float *temperature);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_THERM_H_ */
