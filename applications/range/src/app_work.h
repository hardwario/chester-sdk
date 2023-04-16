#ifndef APP_WORK_H_
#define APP_WORK_H_

#ifdef _cplusplus
extern "C" {
#endif

int app_work_init(void);
void app_work_sample(void);
void app_work_backup_update(void);
void app_work_send(void);
void app_work_aggreg(void);
void app_work_send_with_rate_limit(void);

#ifdef _cplusplus
}
#endif

#endif /* APP_WORK_H */
