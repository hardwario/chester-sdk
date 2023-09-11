#ifndef APP_LAMBRECHT_H_
#define APP_LAMBRECHT_H_

int app_lambrecht_disable(void);
int app_lambrecht_enable(void);
int app_lambrecht_init(void);

int app_lambrecht_read_wind_speed(float *out);
int app_lambrecht_read_wind_direction(float *out);
int app_lambrecht_read_temperature(float *out);
int app_lambrecht_read_humidity(float *out);
int app_lambrecht_read_dew_point(float *out);
int app_lambrecht_read_pressure(float *out);
int app_lambrecht_read_rainfall_total(float *out);
int app_lambrecht_read_rainfall_intensity(float *out);
int app_lambrecht_read_illuminance(float *out);

#endif /* !APP_LAMBRECHT_H_ */
