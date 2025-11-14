/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_tc.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_x3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_tc, CONFIG_CTR_TC_LOG_LEVEL);

static double compute_cold_junction(double Tcj, const double T0, const double V0, const double p1,
				    const double p2, const double p3, const double p4,
				    const double q1, const double q2)
{
#define numerator   ((Tcj - T0) * (p1 + (Tcj - T0) * (p2 + (Tcj - T0) * (p3 + p4 * (Tcj - T0)))))
#define denominator (1.0 + (Tcj - T0) * (q1 + q2 * (Tcj - T0)))
	return (V0 + (numerator / denominator));
#undef numerator
#undef denominator
}

static double compute_temperature_internal(double millivolts, const double T0, const double V0,
					   const double p1, const double p2, const double p3,
					   const double p4, const double q1, const double q2,
					   const double q3)
{
#define numerator                                                                                  \
	((millivolts - V0) *                                                                       \
	 (p1 + (millivolts - V0) * (p2 + (millivolts - V0) * (p3 + p4 * (millivolts - V0)))))
#define denominator                                                                                \
	(1.0 + (millivolts - V0) * (q1 + (millivolts - V0) * (q2 + q3 * (millivolts - V0))))
	return (T0 + (numerator / denominator));
#undef numerator
#undef denominator
}

static double compute_cold_junction_voltage(double temperature_cj)
{
	// Type K equations
	// Tcj = temperature in Celsius
	// Returns equivalent voltage in mV
	const double T0 = 25.0;
	const double V0 = 1.0003453;
	const double p1 = 4.0514854E-02;
	const double p2 = -3.8789638E-05;
	const double p3 = -2.8608478E-06;
	const double p4 = -9.5367041E-10;
	const double q1 = -1.3948675E-03;
	const double q2 = -6.7976627E-05;

	return compute_cold_junction(temperature_cj, T0, V0, p1, p2, p3, p4, q1, q2);
}

static double compute_temperature(double millivolts)
{
	// Type K equations
	// millivolts = voltage in mV
	// Returns computed temperature in Celsius
	double T0, V0, p1, p2, p3, p4, q1, q2, q3;

	if ((-6.404 < millivolts) && (millivolts <= -3.554)) {
		T0 = -1.2147164E+02;
		V0 = -4.1790858E+00;
		p1 = 3.6069513E+01;
		p2 = 3.0722076E+01;
		p3 = 7.7913860E+00;
		p4 = 5.2593991E-01;
		q1 = 9.3939547E-01;
		q2 = 2.7791285E-01;
		q3 = 2.5163349E-02;
	} else if ((-3.554 < millivolts) && (millivolts <= 4.096)) {
		T0 = -8.7935962E+00;
		V0 = -3.4489914E-01;
		p1 = 2.5678719E+01;
		p2 = -4.9887904E-01;
		p3 = -4.4705222E-01;
		p4 = -4.4869203E-02;
		q1 = 2.3893439E-04;
		q2 = -2.0397750E-02;
		q3 = -1.8424107E-03;
	} else if ((4.096 < millivolts) && (millivolts <= 16.397)) {
		T0 = 3.1018976E+02;
		V0 = 1.2631386E+01;
		p1 = 2.4061949E+01;
		p2 = 4.0158622E+00;
		p3 = 2.6853917E-01;
		p4 = -9.7188544E-03;
		q1 = 1.6995872E-01;
		q2 = 1.1413069E-02;
		q3 = -3.9275155E-04;
	} else if ((16.397 < millivolts) && (millivolts <= 33.275)) {
		T0 = 6.0572562E+02;
		V0 = 2.5148718E+01;
		p1 = 2.3539401E+01;
		p2 = 4.6547228E-02;
		p3 = 1.3444400E-02;
		p4 = 5.9236853E-04;
		q1 = 8.3445513E-04;
		q2 = 4.6121445E-04;
		q3 = 2.5488122E-05;
	} else if ((33.275 < millivolts) && (millivolts <= 69.553)) {
		T0 = 1.0184705E+03;
		V0 = 4.1993851E+01;
		p1 = 2.5783239E+01;
		p2 = -1.8363403E+00;
		p3 = 5.6176662E-02;
		p4 = 1.8532400E-04;
		q1 = -7.4803355E-02;
		q2 = 2.3841860E-03;
		q3 = 0.0000000E+00;
	} else {
		return 0; // TODO
	}

	return compute_temperature_internal(millivolts, T0, V0, p1, p2, p3, p4, q1, q2, q3);
}

static double get_temperature(double millivolts, double temperature_cj)
{
	double voltage_cj = compute_cold_junction_voltage(temperature_cj);
	return compute_temperature(millivolts + voltage_cj);
}

int ctr_tc_read(enum ctr_tc_channel channel, enum ctr_tc_type type, float *temperature)
{
	int ret;

	const struct device *dev = NULL;
	enum ctr_x3_channel chan;

	switch (channel) {
	case CTR_TC_CHANNEL_A1:
		LOG_INF("A1");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_a));
		chan = CTR_X3_CHANNEL_1;
		break;
	case CTR_TC_CHANNEL_A2:
		LOG_INF("A2");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_a));
		chan = CTR_X3_CHANNEL_2;
		break;
	case CTR_TC_CHANNEL_B1:
		LOG_INF("B1");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_b));
		chan = CTR_X3_CHANNEL_1;
		break;
	case CTR_TC_CHANNEL_B2:
		LOG_INF("B2");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_b));
		chan = CTR_X3_CHANNEL_2;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	if (!dev || !device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	switch (type) {
	case CTR_TC_TYPE_K:
		break;

	default:
		LOG_ERR("Unknown type: %d", type);
		return -EINVAL;
	}

	/* Cold junction using on-board TMP122 */
	float temperature_cj;
	ret = ctr_therm_read(&temperature_cj);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		return ret;
	}

	int32_t result;
	ret = ctr_x3_measure(dev, chan, &result);
	if (ret) {
		LOG_ERR("Call `ctr_x3_measure` failed: %d", ret);
		return ret;
	}

	float gain = 16.f;
	float div = 2.048 / (1 << 23);
	float div_gain = div / gain;

	float result_v = div_gain * result;

	*temperature = get_temperature(result_v * 1000.f, temperature_cj);

	LOG_INF("Raw: %" PRId32 "; (0x08%" PRIx32 ") U: %.6f V; T: %.6f C", result, result,
		(double)result_v, (double)*temperature);

	/* Result higher than 80mV is out of range */
	if (result_v > 0.080f) {
		LOG_WRN("Voltage out of range.");
		*temperature = NAN;
		return -ERANGE;
	}

	return 0;
}
