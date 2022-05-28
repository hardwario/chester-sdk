/* CHESTER includes */
#include <drivers/ctr_rfmux.h>

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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		ret = ctr_rfmux_acquire(dev);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_acquire` failed: %d", ret);
			k_oops();
		}

		ret = ctr_rfmux_set_interface(dev, CTR_RFMUX_INTERFACE_LTE);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_interface` failed (LTE): %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		ret = ctr_rfmux_set_interface(dev, CTR_RFMUX_INTERFACE_LRW);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_interface` failed (LRW): %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		ret = ctr_rfmux_set_interface(dev, CTR_RFMUX_INTERFACE_NONE);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_interface` failed (NONE): %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		ret = ctr_rfmux_set_antenna(dev, CTR_RFMUX_ANTENNA_INT);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_antenna` failed (INT): %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		ret = ctr_rfmux_set_antenna(dev, CTR_RFMUX_ANTENNA_EXT);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_antenna` failed (EXT): %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));

		ret = ctr_rfmux_set_antenna(dev, CTR_RFMUX_ANTENNA_NONE);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_set_antenna` failed (NONE): %d", ret);
			k_oops();
		}

		ret = ctr_rfmux_release(dev);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_acquire` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(2000));
	}
}
