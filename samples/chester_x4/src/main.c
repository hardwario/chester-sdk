/* CHESTER includes */
#include <drivers/ctr_x4.h>

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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_b));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_1, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_2, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_3, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, true);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));

		ret = ctr_x4_set_output(dev, CTR_X4_OUTPUT_4, false);
		if (ret) {
			LOG_ERR("Call `ctr_x4_set_output` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));
	}
}
