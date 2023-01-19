#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

int app_sensor_iaq_sample(void);
int app_sensor_iaq_aggreg(void);
int app_sensor_iaq_clear(void);

int app_sensor_hygro_sample(void);
int app_sensor_hygro_aggreg(void);
int app_sensor_hygro_clear(void);

int app_sensor_w1_therm_sample(void);
int app_sensor_w1_therm_aggreg(void);
int app_sensor_w1_therm_clear(void);

int app_sensor_rtd_therm_sample(void);
int app_sensor_rtd_therm_aggreg(void);
int app_sensor_rtd_therm_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */
