#ifndef CHESTER_INCLUDE_CTR_SAT_H_
#define CHESTER_INCLUDE_CTR_SAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct {
	int event_code;

	// TODO: Think about content of this structure.
} ctr_sat_message_event;

typedef void (*ctr_sat_message_event_callback)(ctr_sat_message_event* event_data, void* user_data);

int ctr_sat_start();
int ctr_sat_setup_wifi_devkit(char* ssid, char* password, char* api_key);
int ctr_sat_send_message(uint8_t* message, size_t message_size, ctr_sat_message_event_callback event_callback, void* event_callback_user_data);
int ctr_sat_revoke_all_messages();
int ctr_sat_stop();

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_SAT_H_ */
