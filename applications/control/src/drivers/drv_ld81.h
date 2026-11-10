/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRV_LD81_H_
#define DRV_LD81_H_

#include "../app_sensor_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Jablotron LD-81 conductivity flood detector (type_id = 1). */
extern const struct app_sensor_driver ld81_driver;

/* Fail-safe ceiling. A disconnected sensor, a broken cable or a terminal that has
 * come loose leaves the measurement node pulled up towards the excitation rail —
 * i.e. ABOVE the flood threshold — so the old code reported every such fault as
 * FLOODED. Treat a reading at or above this as an open circuit and mark the slot
 * invalid instead, which reuses the existing invalid path (LRW status bit0 clear
 * plus 0xFFFF mV, CBOR nil) and so needs no codec change.
 *
 * 3500 mV sits well above the calibrated flooded level (~1690 mV) and well below
 * the ~4.6 V an unloaded node reaches off the 5 V rail. Revisit together with the
 * threshold if the excitation resistor changes, since both scale with it. */
#define LD81_OPEN_CIRCUIT_MV 3500

/* Fail-safe floor, and the more important of the two for a flood alarm. The
 * ceiling above catches a broken cable, which fails towards FLOODED and so is
 * noisy but safe. A sense line shorted to GND, a shorted sensor or a dead ADC
 * reads ~0 mV, which is comfortably below the threshold and would be reported as
 * `dry` for as long as it lasts — the failure that actually matters, because it
 * silently disables the alarm.
 *
 * Dry is calibrated at ~770 mV and the node is held up by the excitation
 * resistor, so it cannot legitimately approach zero while the sensor is
 * connected. 300 mV leaves a wide margin under the dry level. */
#define LD81_SHORT_CIRCUIT_MV 300

/* Hysteresis below the threshold. A slot turns flooded above the threshold, but only
 * turns dry again at or below `threshold - LD81_HYSTERESIS_MV`, so a reading that
 * sits right at the threshold cannot flip the state, and raise an alarm, on every
 * sample. 100 mV is small against the ~900 mV between the calibrated dry and flooded
 * levels. */
#define LD81_HYSTERESIS_MV 100

/* The range `sensor-ld81-threshold` accepts. The threshold and the hysteresis band
 * under it must both sit inside the fail-safe window: a threshold outside it can never
 * be crossed, because the window's guards fire first and fault the slot, and a band
 * reaching below the floor would leave a flooded slot no reading that turns it dry.
 * Flood means strictly above the threshold, so the highest usable threshold is two
 * below the open-circuit ceiling. */
#define LD81_THRESHOLD_MIN_MV (LD81_SHORT_CIRCUIT_MV + LD81_HYSTERESIS_MV + 1)
#define LD81_THRESHOLD_MAX_MV (LD81_OPEN_CIRCUIT_MV - 2)

/* A fault has to be seen on this many samples in a row before it is reported. The slot
 * is marked invalid on the first faulty sample, so a bad reading is never passed on;
 * only the alarm waits. That keeps a terminal that is loose, and flips between a valid
 * reading and an open circuit, from raising an alarm on every flip. */
#define LD81_FAULT_CONFIRM_SAMPLES 2

/* At most one fault alarm per slot in this time. A terminal that keeps dropping out for
 * a few samples at a time would otherwise raise one per episode and use up the event
 * rate limit it shares with the triggers and power events. The data still goes invalid
 * on every faulty sample, so the periodic reports show the faults as they happen. */
#define LD81_FAULT_ALARM_HOLDOFF_S 3600

/* Decoded LD-81 result. Storage is private to drv_ld81.c (one entry per sensor
 * slot); consumers (app_cbor.c / app_lrw.c / shell) obtain a snapshot through the
 * getter below and must respect `valid`. */
struct app_data_ld81 {
	bool valid;
	int voltage_mv;
	bool flooded;
};

/* Snapshot the latest decoded data for a sensor slot (0-based) into *out.
 * Returns 0, or -EINVAL if the slot is out of range.
 *
 * This copies rather than returning a pointer on purpose. The sampler writes
 * `voltage_mv`, `flooded` and `valid` as a set, from the work queue, while the
 * encoders read them from another thread; handing out a pointer to the live struct
 * let a caller read the three fields separately and straddle an update, producing a
 * payload marked valid that pairs one sample's millivolts with another's flood
 * state. Copying under the driver's lock makes a reader see one consistent set. */
int ld81_get_data(int slot, struct app_data_ld81 *out);

#ifdef __cplusplus
}
#endif

#endif /* DRV_LD81_H_ */
