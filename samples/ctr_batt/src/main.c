#include <devicetree.h>
#include <drivers/ctr_batt.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));
	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return;
	}

	while (1) {
		int rest_mv;
		ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv,
		                                   K_MSEC(CTR_BATT_REST_TIMEOUT_DEFAULT_MS));
		if (ret < 0) {
			LOG_WRN("Call `ctr_batt_get_rest_voltage` failed: %d", ret);
		}

		int load_mv;
		ret = ctr_batt_get_load_voltage_mv(dev, &load_mv,
		                                   K_MSEC(CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS));
		if (ret < 0) {
			LOG_WRN("Call `ctr_batt_get_load_voltage` failed: %d", ret);
		}

		int current_ma;
		ctr_batt_get_load_current_ma(dev, &current_ma, load_mv);

		LOG_INF("Battery rest voltage = %u mV", rest_mv);
		LOG_INF("Battery load voltage = %u mV", load_mv);
		LOG_INF("Battery load current = %u mA", current_ma);

		k_sleep(K_SECONDS(60));
	}
}
