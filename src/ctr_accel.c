#include <ctr_accel.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_accel, LOG_LEVEL_DBG);

#define GRAVITY 9.80665f
#define ORIENTATION_THR 0.4f

static const int m_vectors[7][3] = {
	[0] = { 0, 0, 0 },  [1] = { -1, 0, 0 }, [2] = { 0, 0, 1 }, [3] = { 0, 1, 0 },
	[4] = { 0, -1, 0 }, [5] = { 0, 0, -1 }, [6] = { 1, 0, 0 },
};

static int m_orientation;

static K_MUTEX_DEFINE(m_mut);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));

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

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	/* TODO Check if this is the best aprroach */
	for (int i = 0; i < 5; i++) {
		ret = sensor_sample_fetch(m_dev);

		if (ret != ENODATA) {
			break;
		}

		k_sleep(K_MSEC(10));
	}

	if (ret < 0) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val[3];

	ret = sensor_channel_get(m_dev, SENSOR_CHAN_ACCEL_XYZ, val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float tmp_accel_x = sensor_value_to_double(&val[0]);
	float tmp_accel_y = sensor_value_to_double(&val[1]);
	float tmp_accel_z = sensor_value_to_double(&val[2]);

	LOG_DBG("Acceleration X: %.3f m/s^2", tmp_accel_x);
	LOG_DBG("Acceleration Y: %.3f m/s^2", tmp_accel_y);
	LOG_DBG("Acceleration Z: %.3f m/s^2", tmp_accel_z);

	if (accel_x != NULL) {
		*accel_x = tmp_accel_x;
	}

	if (accel_y != NULL) {
		*accel_y = tmp_accel_y;
	}

	if (accel_z != NULL) {
		*accel_z = tmp_accel_z;
	}

	update_orientation(tmp_accel_x / GRAVITY, tmp_accel_y / GRAVITY, tmp_accel_z / GRAVITY);

	int tmp_orientation = m_orientation;

	LOG_DBG("Orientation: %d", tmp_orientation);

	if (orientation != NULL) {
		*orientation = tmp_orientation;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_accel_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float accel_x;
	float accel_y;
	float accel_z;
	int orientation;

	ret = ctr_accel_read(&accel_x, &accel_y, &accel_z, &orientation);

	if (ret < 0) {
		LOG_ERR("Call `ctr_accel_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "accel-axis-x: %.3f m/s^2", accel_x);
	shell_print(shell, "accel-axis-y: %.3f m/s^2", accel_y);
	shell_print(shell, "accel-axis-z: %.3f m/s^2", accel_z);
	shell_print(shell, "orientation: %d", orientation);

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_accel,
                               SHELL_CMD_ARG(read, NULL, "Read sensor data.", cmd_accel_read, 1, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(accel, &sub_accel, "Accelerometer commands.", print_help);
