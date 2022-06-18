/* CHESTER includes */
#include <ctr_accel.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_accel, CONFIG_CTR_ACCEL_LOG_LEVEL);

#define GRAVITY 9.80665f
#define ORIENTATION_THR 0.4f

static const int m_vectors[7][3] = {
	[0] = { 0, 0, 0 },  [1] = { -1, 0, 0 }, [2] = { 0, 0, 1 }, [3] = { 0, 1, 0 },
	[4] = { 0, -1, 0 }, [5] = { 0, 0, -1 }, [6] = { 1, 0, 0 },
};

static K_MUTEX_DEFINE(m_mut);
static ctr_accel_user_cb m_user_cb;
static void *m_user_data;
static int m_orientation;

int ctr_accel_set_event_handler(ctr_accel_user_cb user_cb, void *user_data)
{
	k_mutex_lock(&m_mut, K_FOREVER);

	m_user_cb = user_cb;
	m_user_data = user_data;

	k_mutex_unlock(&m_mut);

	return 0;
}

int ctr_accel_set_threshold(float threshold, int duration)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	struct sensor_value val = {
		.val1 = duration,
		.val2 = 0,
	};

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_DUR, &val);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	int64_t threshold_ = threshold * 1000000;

	val.val1 = threshold_ / 1000000;
	val.val2 = threshold_ % 1000000;

	ret = sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH, &val);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_mutex_unlock(&m_mut);

	return ret;
}

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	k_mutex_lock(&m_mut, K_FOREVER);

	if (m_user_cb) {
		m_user_cb(CTR_ACCEL_EVENT_TILT, m_user_data);
	}

	k_mutex_unlock(&m_mut);
}

int ctr_accel_enable_trigger(void)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DELTA,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};

	ret = sensor_trigger_set(dev, &trig, trigger_handler);
	if (ret != 0) {
		LOG_ERR("Call `sensor_trigger_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_mutex_unlock(&m_mut);

	return 0;
}

static void update_orientation(float coeff_x, float coeff_y, float coeff_z)
{
	int vector_x = m_vectors[m_orientation][0];
	int vector_y = m_vectors[m_orientation][1];
	int vector_z = m_vectors[m_orientation][2];

	if ((vector_x == 0 && (coeff_x < -ORIENTATION_THR || coeff_x > ORIENTATION_THR)) ||
	    (vector_x == 1 && (coeff_x < 1.f - ORIENTATION_THR)) ||
	    (vector_x == -1 && (coeff_x > -1.f + ORIENTATION_THR))) {
		goto update;
	}

	if ((vector_y == 0 && (coeff_y < -ORIENTATION_THR || coeff_y > ORIENTATION_THR)) ||
	    (vector_y == 1 && (coeff_y < 1.f - ORIENTATION_THR)) ||
	    (vector_y == -1 && (coeff_y > -1.f + ORIENTATION_THR))) {
		goto update;
	}

	if ((vector_z == 0 && (coeff_z < -ORIENTATION_THR || coeff_z > ORIENTATION_THR)) ||
	    (vector_z == 1 && (coeff_z < 1.f - ORIENTATION_THR)) ||
	    (vector_z == -1 && (coeff_z > -1.f + ORIENTATION_THR))) {
		goto update;
	}

	return;

update:

	for (int i = 1; i <= 6; i++) {
		float delta_x = fabsf(m_vectors[i][0] - coeff_x);
		float delta_y = fabsf(m_vectors[i][1] - coeff_y);
		float delta_z = fabsf(m_vectors[i][2] - coeff_z);

		if (delta_x < 1.f - ORIENTATION_THR && delta_y < 1.f - ORIENTATION_THR &&
		    delta_z < 1.f - ORIENTATION_THR) {
			m_orientation = i;
			return;
		}
	}
}

int ctr_accel_read(float *accel_x, float *accel_y, float *accel_z, int *orientation)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	/* TODO Check if this is the best aprroach */
	for (int i = 0; i < 5; i++) {
		ret = sensor_sample_fetch(dev);

		if (ret != ENODATA) {
			break;
		}

		k_sleep(K_MSEC(10));
	}

	if (ret) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val[3];
	ret = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, val);
	if (ret) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float accel_x_ = sensor_value_to_double(&val[0]);
	float accel_y_ = sensor_value_to_double(&val[1]);
	float accel_z_ = sensor_value_to_double(&val[2]);

	LOG_DBG("Acceleration X: %.3f m/s^2", accel_x_);
	LOG_DBG("Acceleration Y: %.3f m/s^2", accel_y_);
	LOG_DBG("Acceleration Z: %.3f m/s^2", accel_z_);

	if (accel_x != NULL) {
		*accel_x = accel_x_;
	}

	if (accel_y != NULL) {
		*accel_y = accel_y_;
	}

	if (accel_z != NULL) {
		*accel_z = accel_z_;
	}

	update_orientation(accel_x_ / GRAVITY, accel_y_ / GRAVITY, accel_z_ / GRAVITY);

	int orientation_ = m_orientation;

	LOG_DBG("Orientation: %d", orientation_);

	if (orientation != NULL) {
		*orientation = orientation_;
	}

	k_mutex_unlock(&m_mut);

	return 0;
}
