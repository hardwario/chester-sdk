/* CHESTER includes */
#include <ctr_rtd.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		float temperature;
		ret = ctr_rtd_read(CTR_RTD_CHANNEL_B1, CTR_RTD_TYPE_PT1000, &temperature);
		if (ret) {
			LOG_ERR("Call `ctr_rtd_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Temperature: %.3f C", temperature);

		k_sleep(K_SECONDS(1));
	}
}
