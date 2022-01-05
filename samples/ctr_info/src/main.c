#include <ctr_info.h>

#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(app);

void main(void)
{
	int ret;

	uint32_t sn;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_info_get_serial_number(&sn);
	if (ret) {
		LOG_WRN("Failed to get serial number: %d", ret);
	} else {
		LOG_INF("Serial number: %u", sn);
	}
}
