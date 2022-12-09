#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

int app_sensor_voltage_sample(void);
int app_sensor_voltage_aggreg(void);
void app_sensor_voltage_clear(void);

int app_sensor_current_sample(void);
int app_sensor_current_aggreg(void);
void app_sensor_current_clear(void);

int app_sensor_counter_aggreg(void);
void app_sensor_counter_clear(void);

void app_sensor_trigger_clear(void);

#if defined(CONFIG_SHIELD_CTR_S2)
int app_sensor_hygro_sample(void);
int app_sensor_hygro_aggreg(void);
void app_sensor_hygro_clear(void);
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */
