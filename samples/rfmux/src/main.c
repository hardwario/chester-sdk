/* CHESTER includes */
#include <drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *rfmux_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Setting RF interface to LTE");
		ret = ctr_rfmux_set_interface(rfmux_dev, CTR_RFMUX_INTERFACE_LTE);
		if (ret) {
			LOG_ERR("Setting RF interface to LTE failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		LOG_INF("Setting RF interface to LRW");
		ret = ctr_rfmux_set_interface(rfmux_dev, CTR_RFMUX_INTERFACE_LRW);
		if (ret) {
			LOG_ERR("Setting RF interface to LRW failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		LOG_INF("Setting RF interface to NONE");
		ret = ctr_rfmux_set_interface(rfmux_dev, CTR_RFMUX_INTERFACE_NONE);
		if (ret) {
			LOG_ERR("Setting RF interface to NONE failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		LOG_INF("Setting RF antenna to INT");
		ret = ctr_rfmux_set_antenna(rfmux_dev, CTR_RFMUX_ANTENNA_INT);
		if (ret) {
			LOG_ERR("Setting RF antenna to INT failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		LOG_INF("Setting RF antenna to EXT");
		ret = ctr_rfmux_set_antenna(rfmux_dev, CTR_RFMUX_ANTENNA_EXT);
		if (ret) {
			LOG_ERR("Setting RF antenna to EXT failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		LOG_INF("Setting RF antenna to NONE");
		ret = ctr_rfmux_set_antenna(rfmux_dev, CTR_RFMUX_ANTENNA_NONE);
		if (ret) {
			LOG_ERR("Setting RF antenna to NONE failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));
	}
}
