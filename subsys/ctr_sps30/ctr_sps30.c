/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/sensor/sps30.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>
#include <math.h>

LOG_MODULE_REGISTER(ctr_sps30, CONFIG_CTR_SPS30_LOG_LEVEL);

static K_MUTEX_DEFINE(m_mut);

static const struct device *m_sens_dev = DEVICE_DT_GET(DT_NODELABEL(sps30_ext));
static const struct device *m_x0_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

static int power_on()
{
	int ret = ctr_x0_set_mode(m_x0_dev, CTR_X0_CHANNEL_1, CTR_X0_MODE_PWR_SOURCE);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(1000));

	ret = sps30_power_on_init(m_sens_dev);
	if (ret) {
		LOG_ERR("Call `sps30_power_on_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int power_off()
{
	int ret = ctr_x0_set_mode(m_x0_dev, CTR_X0_CHANNEL_1, CTR_X0_MODE_DEFAULT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_sps30_read(float *mass_conc_pm_1_0, float *mass_conc_pm_2_5, float *mass_conc_pm_4_0,
		   float *mass_conc_pm_10_0, float *num_conc_pm_0_5, float *num_conc_pm_1_0,
		   float *num_conc_pm_2_5, float *num_conc_pm_4_0, float *num_conc_pm_10_0)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!device_is_ready(m_sens_dev)) {
		LOG_ERR("Device `SPS30_EXT` not ready");
		ret = -EINVAL;
		goto exit;
	}

	ret = power_on();
	if (ret) {
		LOG_ERR("Call `power_on` failed: %d", ret);
		goto exit;
	}

	k_sleep(K_MSEC(15000));

	ret = sensor_sample_fetch(m_sens_dev);
	if (ret) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		goto exit;
	}

	struct sensor_value val;

	if (mass_conc_pm_1_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_PM_1_0, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*mass_conc_pm_1_0 = sensor_value_to_double(&val);
		LOG_DBG("Particulate Matter 1.0: %.2f ug/m3", (double)*mass_conc_pm_1_0);
	}

	if (mass_conc_pm_2_5) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_PM_2_5, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*mass_conc_pm_2_5 = sensor_value_to_double(&val);
		LOG_DBG("Particulate Matter 2.5: %.2f ug/m3", (double)*mass_conc_pm_1_0);
	}

	if (mass_conc_pm_4_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_PM_4_0, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*mass_conc_pm_4_0 = sensor_value_to_double(&val);
		LOG_DBG("Particulate Matter 4.0: %.2f ug/m3", (double)*mass_conc_pm_1_0);
	}

	if (mass_conc_pm_10_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_PM_10, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*mass_conc_pm_10_0 = sensor_value_to_double(&val);
		LOG_DBG("Particulate Matter 10.0: %.2f ug/m3", (double)*mass_conc_pm_1_0);
	}

	if (num_conc_pm_0_5) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_0_5,
					 &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*num_conc_pm_0_5 = sensor_value_to_double(&val);
		LOG_DBG("Number concentration 0.5: %.2f", (double)*num_conc_pm_0_5);
	}

	if (num_conc_pm_1_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_1_0,
					 &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*num_conc_pm_1_0 = sensor_value_to_double(&val);
		LOG_DBG("Number concentration 1.0: %.2f", (double)*num_conc_pm_1_0);
	}

	if (num_conc_pm_2_5) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_2_5,
					 &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*num_conc_pm_2_5 = sensor_value_to_double(&val);
		LOG_DBG("Number concentration 2.5: %.2f", (double)*num_conc_pm_2_5);
	}

	if (num_conc_pm_4_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_4_0,
					 &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*num_conc_pm_4_0 = sensor_value_to_double(&val);
		LOG_DBG("Number concentration 4.0: %.2f", (double)*num_conc_pm_4_0);
	}

	if (num_conc_pm_10_0) {
		ret = sensor_channel_get(m_sens_dev, SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_10_0,
					 &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			goto exit;
		}
		*num_conc_pm_10_0 = sensor_value_to_double(&val);
		LOG_DBG("Number concentration 10.0: %.2f", (double)*num_conc_pm_10_0);
	}

exit:
	if (ret) {
		if (mass_conc_pm_1_0) {
			*mass_conc_pm_1_0 = NAN;
		}
		if (mass_conc_pm_2_5) {
			*mass_conc_pm_2_5 = NAN;
		}
		if (mass_conc_pm_4_0) {
			*mass_conc_pm_4_0 = NAN;
		}
		if (mass_conc_pm_10_0) {
			*mass_conc_pm_10_0 = NAN;
		}

		if (num_conc_pm_0_5) {
			*num_conc_pm_0_5 = NAN;
		}
		if (num_conc_pm_1_0) {
			*num_conc_pm_1_0 = NAN;
		}
		if (num_conc_pm_2_5) {
			*num_conc_pm_2_5 = NAN;
		}
		if (num_conc_pm_4_0) {
			*num_conc_pm_4_0 = NAN;
		}
		if (num_conc_pm_10_0) {
			*num_conc_pm_10_0 = NAN;
		}
	}

	int pwr_ret = power_off();
	if (pwr_ret) {
		LOG_WRN("Call `power_off` failed: %d", ret);
	}

	k_mutex_unlock(&m_mut);
	return ret;
}

static int init(void)
{
	int ret;

	LOG_INF("SPS30 subystem initialization");

	if (!device_is_ready(m_x0_dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	/*
		TODO: in the current Zephyr version provided by NCS, there is no support for
		deferred initialization

		This is supported in the latest Zephyr version (starting at Zephyr 3.7.0) and
		appears in future versions of NCS. Once this happens, uncomment the following
		to initialize the device after it has been manually powered on:

		ret = power_on();
		if (ret) {
			LOG_ERR("Call `power_on` failed: %d", ret);
			k_oops();
		}
		device_init(m_sens_dev);
	*/

	ret = power_off();
	if (ret) {
		LOG_WRN("Call `power_off` failed: %d", ret);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
