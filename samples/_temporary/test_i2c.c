#include "test_i2c.h"
#include <ctr_bsp.h>
#include <ctr_bus_i2c.h>
#include <ctr_drv_sht30.h>
#include <ctr_drv_tmp112.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_i2c, LOG_LEVEL_DBG);

int test_i2c(void)
{
	int ret;

	LOG_INF("Start");

	for (;;) {
#if 1
		LOG_INF("Reading TMP112...");

		float tmp112_t;

		ret = hio_bsp_tmp112_measure(&tmp112_t);

		if (ret < 0) {
			LOG_ERR("Call `hio_bsp_tmp112_measure` failed: %d", ret);
		} else {
			LOG_INF("TMP112 temperature = %.2f C", tmp112_t);
		}
#endif

#if 1
		LOG_INF("Reading SHT30...");

		float sht30_t, sht30_rh;

		ret = hio_bsp_sht30_measure(&sht30_t, &sht30_rh);

		if (ret < 0) {
			LOG_ERR("Call `hio_bsp_sht30_measure` failed: %d", ret);
		} else {
			LOG_INF("SHT30 temperature = %.2f C", sht30_t);
			LOG_INF("SHT30 humidity = %.1f %%", sht30_rh);
		}
#endif

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
