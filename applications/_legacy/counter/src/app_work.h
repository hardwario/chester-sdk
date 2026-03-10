#ifndef APP_WORK_H_
#define APP_WORK_H_

int app_work_init(void);
void app_work_backup_update(void);
void app_work_sample(void);
void app_work_send(void);
void app_work_send_with_rate_limit(void);

#endif /* APP_WORK_H_ */