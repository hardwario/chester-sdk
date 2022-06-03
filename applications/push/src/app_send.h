#ifndef APP_SEND_H_
#define APP_SEND_H_

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_timer g_app_send_timer;

int app_send(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SEND_H_ */
