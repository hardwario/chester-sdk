/* CHESTER includes */
#include <ctr_gnss.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void gnss_event_handler(enum ctr_gnss_event event, union ctr_gnss_event_data *data, void *user_data)
{
	if (event == CTR_GNSS_EVENT_UPDATE) {
		LOG_DBG("Fix quality: %d", data->update.fix_quality);
		LOG_DBG("Satellites tracked: %d", data->update.satellites_tracked);
		LOG_DBG("Latitude: %.7f", data->update.latitude);
		LOG_DBG("Longitude: %.7f", data->update.longitude);
		LOG_DBG("Altitude: %.1f", data->update.altitude);
	}
}

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ctr_gnss_set_event_cb(gnss_event_handler, NULL);

	for (;;) {
		k_sleep(K_FOREVER);
	}
}
