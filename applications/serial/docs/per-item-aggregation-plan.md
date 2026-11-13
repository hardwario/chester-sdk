# Slot-aware drivers, per-item aggregation, SenseCAP and Cubic 6303 drivers

Status: implemented on `matej/serial-meteo-aggregation`, on top of MR !881 (`feat/serial-uart-rppu`: the
`uart` serial mode, the Piketronic RPP-U driver and the LoRaWAN payload v2). Target application:
`applications/serial`. This document started as the plan of MR !870; it now describes what was built and how
the review of the plan was addressed. §7 describes how the change fits the work of !881.

## Context

CHESTER Serial reported every driver value as a raw per-sample record, and every driver kept its data in
module statics. The goal was to let a driver declare, per measured item, how the item is aggregated over the
`interval-aggreg` window, and to use that first for the Seeed **SenseCAP S1000 / S700 / S500** weather
stations and the **Cubic 6303 / OPM6303** particulate sensor, until now real drivers only in
`applications/meteo` (without the S700).

The review of the plan (five points and a sketch of a table-driven driver) showed that three of its concerns
already affected the ten existing drivers:

- Two devices of the same type on different addresses shared one buffer: the samples of both were mixed and
  every slot encoded the same list.
- In LoRaWAN and NONE mode the sample buffers were never cleared and stayed full.
- A report larger than the 8 KB CBOR buffer failed to encode, nothing was cleared and every later report
  failed the same way until reboot.

So the change is broader than the plan: all drivers moved to per-slot storage, and the send path was reworked.
The legacy wire formats (CBOR `devices[].data` list, LoRaWAN device data) of the existing drivers are
unchanged. The LoRaWAN payload v2 of !881, which packs all devices into one uplink, carries the table-driven
devices too (§5).

## Review points and how they are addressed

| # | Review point | Result |
|---|---|---|
| 1 | Item storage per slot, not per driver type; one shared pool sized from the intervals; reject configs that do not fit | Done for all drivers (§1, §2). The pool (17 152 B) replaces the static sample buffers the drivers held whether configured or not. `app config save` rejects a configuration whose base buffers do not fit and warns where a device keeps less than one report period, then saves and reboots. On the cloud shell it only validates (a configuration from the cloud is saved after its last line). The global `config save` and a configuration from the cloud skip the check; the boot then sizes the rings down instead of losing devices. |
| 2 | Generic `app_modbus` readers instead of local helpers | Done (§3): typed readers with function code and word order, and shared decoders. The local decoders of the drivers are gone; the static stub readers were removed with the stubs. |
| 3 | LoRaWAN encoding; Float16 range and resolution; buffers never cleared outside LTE | Done (§5). One value per item from the last closed window. Pressure goes as U16 in 0.1 hPa, not Float16 hPa, whose step at 1013 hPa is 0.5–1 hPa. Light goes as Float16 klux, the rainfall total as U32. S1000 needs 22 B of data (not 20, the counter is 4 B), S500 and Cubic 10 B. Buffers are dropped after the LoRaWAN uplink (the newest window is kept) and in NONE mode. |
| 4 | Aggregation modes STATS / LAST / COUNTER / CIRCULAR | Done (§2), including the vector mean for wind direction. COUNTER and LAST give the same value; they differ in the LoRaWAN encoding (U32). The sketch's single `scale` used for reading and encoding does not work for float registers (a kWh float register must not be divided), so items carry a read divisor (`div`, integer formats only) and a wire multiplier (`mul`). |
| 5 | Send can get stuck; worst-case size and fallback | Done (§4): oversized reports are split into several messages, sent ranges are dropped exactly, an entry that cannot fit is dropped with an error. Like the other applications, the report is dropped after the send attempt even when it fails; delivery after a failure is left to the cloud spool. |
| Sketch | Table-driven drivers with optional `init` / `sample` / derived items / own protocol | Implemented as sketched (§1), plus a probe slot for shell commands that address a device directly. |

## 1. Driver interface (`src/drivers/drv_interface.h`, `src/app_device.c`)

```c
struct app_device_driver {
	const char *name;
	enum app_device_type type;
	enum app_device_transport transport; /* UART or MODBUS (!881), see §7 */

	uint8_t max_instances; /* 0 = no limit; MicroSENS, Promag and RPP-U: 1 (one UART) */
	size_t ctx_size;       /* driver private state, slot->ctx */
	size_t rec_size;       /* raw record per sample, slot->rec (0 = none) */
	uint16_t rec_fixed;    /* fixed record capacity (Promag card reads: 32) */

	const struct app_driver_item *items; /* item table (table-driven drivers) */
	size_t item_count;
	enum app_modbus_fc fc;               /* FC03 / FC04 of the generic item read */
	uint32_t codec_key;                  /* CODEC_KEY_E_DEVICES__<GROUP> */

	int (*init)(struct app_driver_slot *slot);   /* retried before a sample until it succeeds */
	int (*sample)(struct app_driver_slot *slot); /* NULL with items: generic table read */
	int (*deinit)(struct app_driver_slot *slot);
	int (*calibrate)(struct app_driver_slot *slot, enum app_device_calibration, int value);
	int (*reset)(struct app_driver_slot *slot);
	int (*config)(struct app_driver_slot *slot, uint8_t new_addr, const char *parity);
	void (*print_data)(const struct shell *shell, struct app_driver_slot *slot);
};
```

- A slot (`struct app_driver_slot`) carries the configuration index, the Modbus address, the driver, a lock,
  the driver state, the record ring and the item store. All callbacks run under the slot lock.
- A table-driven Modbus device is an item table only (SenseCAP, Cubic). `app_device_sample_items()` is the
  generic read; a custom `.sample` can call it and add computed items (`reg = APP_DRIVER_REG_NONE`) with
  `app_driver_items_put()`. Drivers with their own protocol (MicroSENS, Promag) keep their `.sample` and
  records.
- `device <driver> sample <addr>` and `config <addr> <new_addr>` run on a probe slot (`app_device_probe_*`):
  they no longer add samples to the upload buffer of a configured device or change its address in memory.
- `device sensecap sample <addr> [s1000|s700|s500]` reads the S1000 table (default), the nine S700 items
  or the five S500 items.
- `device list` shows the ring fill and the dropped entries per device. In LoRaWAN / NONE mode, where the
  rings hold 2 entries, `dropped` counts every overwritten entry; that is expected, only the latest data is
  sent.
- A driver keeps either an item table or raw records, not both: reports encode one of them, and a device
  whose driver has both is ignored at boot.
- Lock order: `app_data_lock` → slot lock → Modbus bus.

## 2. Per-slot store (`src/app_driver_store.{c,h}`)

**Pool and sizing.**
- One pool of 17 152 B, carved per configured slot at boot; every block is 8-byte aligned. It grew by the
  static buffers each driver gave up (the RPP-U of !881: 768 B).
- `app_driver_pool_plan()` sizes the rings for one report period plus one entry:
  - records: `interval-report / interval-sample`, clamped to 2..64 (32 with `interval-report` 0);
  - windows: `interval-report / interval-aggreg`, clamped to 2..32.
- In LoRaWAN / NONE mode the rings hold 2 entries, as only the latest data is sent.
- Leftover pool extends the rings, up to 255 entries, for LTE outages.
- If even the base does not fit, `app config save` rejects the configuration. `config save` and a
  configuration from the cloud do not check it: at boot the rings then shrink proportionally (at least 2
  entries) with a warning, and a device whose minimum does not fit is left out with an error.
- The rings are capped, so a long report interval with a short sample interval may not fit even with the
  leftover pool. `app config save` and the boot log then warn how many of a report period's samples or
  windows a device keeps.
- In LTE mode Promag card reads keep a fixed ring of 32 records, which is neither grown nor shrunk; in
  LoRaWAN / NONE mode it holds 2 like the other rings.

**Rings.**
- Records and windows use monotonic sequence numbers. A full ring drops the oldest entry and counts it.
  Before, the newest sample was dropped.
- `*_pop_until(end)` removes exactly what a sent message carried, also if a sample arrived or the oldest
  entry was dropped in between.

**Items.** Each item keeps:
- its last valid sample;
- running count / min / max / sum (STATS) or the sine and cosine sums (CIRCULAR);
- a buffer of up to 32 samples for the median;
- a ring of closed windows.

Failed reads (NaN) are not stored, so one timeout does not void a window.

| Mode | Window output | Used for |
|---|---|---|
| `LAST` | last sample | status values |
| `STATS` | min, max, avg (over all samples), median (over the first 32) | temperature, humidity, pressure, PM, CO2, gas flow |
| `COUNTER` | last absolute value | accumulated rainfall; the cloud computes differences, a lost message loses nothing |
| `CIRCULAR` | vector mean in degrees, no value if the directions cancel out | wind direction |

**Timeline.**
- A window's timestamp is `base_ts + seq * interval-aggreg`.
- Windows are counted by the expirations of the aggregation timer (`k_timer_status_get()`), not by the time
  between closes. When the work queue was blocked longer than one interval (aggregation ticks coalesced
  behind a long LTE send), the missed windows are inserted empty, so the `$tsp` timeline stays aligned; a
  close that is merely late adds no window. If more windows were missed than the ring holds, the ring
  restarts with the current window.
- The timeline is re-anchored when the ring is empty (after a report) and when the RTC moved differently
  from the uptime (the clock was set); the windows in the ring are kept, after the missed windows are
  inserted.

The module has no dependency on the application configuration or generated headers. It is covered by the
ztest suite `tests/applications/serial/driver_store`.

## 3. Modbus access (`src/app_modbus.{c,h}`, `src/app_modbus_decode.h`)

- `app_modbus_read_{u16,s16,u32,s32,float,value}(addr, reg, fc, [word order | format], out)` with
  `APP_MODBUS_FC_HOLDING` / `APP_MODBUS_FC_INPUT`. The decoders are pure functions in `app_modbus_decode.h`.
- `app_modbus_acquire()` / `app_modbus_release()` hold the bus, recursively, for a whole driver operation, so
  the shell and the work queue never interleave transactions. `app_modbus_enable()` / `_disable()` are
  reference-counted.
- Simple meters read a register table into their state with `app_device_read_regs()`. Device-specific
  sequences stay in the drivers:
  - iEM3000 INT64 energy;
  - FlowT gaps and mantissa / exponent totalizers;
  - Piketronic block reads;
  - the SolaX two-register limit.
- `app_modbus_sample()` handles only configured devices without a driver (the Lambrecht and generic stubs).
  Shell `modbus sample` points to `sample` / `device sample <idx>` when there is no such device.

## 4. Reports (`src/app_send.c`, `src/app_send_split.{c,h}`, `src/app_cbor.c`)

**Encoding.**
- `app_cbor_encode(zs, sel)` encodes a selection: per device the range of records and windows, whether the
  BLE tag measurements are included, and the message sequence number.
- Every message lists all configured devices.
- A full payload buffer is reported as `-ENOSPC`.
- The sequence number increments only for messages actually sent.

**LTE.**
- The whole report is tried first.
- If it does not fit the 8 KB buffer, `app_send_split()` packs it into several messages:
  - ranges are added in device order with the largest part that fits next to what the message already holds;
  - the BLE tags go last;
  - an entry that does not fit an empty message is dropped with an error log.
- The first failed send stops the report. The report is dropped anyway (no retry in the application, as in
  the other applications); a failed message is delivered later by the cloud spool (`CONFIG_CTR_CLOUD_SPOOL`),
  messages of the report that were not attempted are lost.
- The split logic is covered by `tests/applications/serial/send_split`.

**After sending.**
- Everything the report selected is dropped, sent or not.
- The BLE tag measurements are dropped once the message that carried them was sent; before, they were
  cleared after the whole send.

**LoRaWAN / NONE.**
- An uplink (payload v2) carries as many devices as fit 51 B; `app_lrw_encode_multi()` reports which. After
  the uplink is sent, the records and all but the newest closed window of every device in it are dropped.
  Later uplinks send that window again until the next one closes; they do not fall back to a single,
  possibly stale, sample. A failed uplink drops nothing; the rings overwrite their oldest entry.
- In NONE mode the buffers are dropped on every report tick.

## 5. Item encoding and codec

**CBOR.** A table-driven device reports a map under its group, one `$tsp` list per item, as BLE tags and
Meteo do:

```
devices[i] = { device, type, type_name, addr,
  <group>: { <item>: { measurements: [timestamp, interval, min, max, avg, mdn, ...] } } }   STATS
  <group>: { <item>: { measurements: [timestamp, interval, value, ...] } }                 others
```

- Values are `value * mul` rounded to the nearest int32 (`mul` = `$div` of the codec), nil if missing or out
  of the int32 range (`app_driver_value_to_int32()`). The BLE tag temperature and humidity use the same
  conversion.
- `codec/cbor-decoder.yaml` adds the groups `sensecap` (Meteo item names, `$div`, `$fpp`) and `cubic_6303`
  after `data`.
- The keys after them (`ble_tags`) renumber. The decoder is uploaded by the firmware and bound by its hash,
  so CBOR stays consistent.

**LoRaWAN.**
- One value per item, in table order, from the last closed window (the last sample before the first window
  closes): STATS / CIRCULAR send the average, LAST / COUNTER the value.
- Encoding: Float16 by default, U32 of `value * mul` for counters, or per item a scaled U16 / U32 / Float16.
- Unavailable values: Float16 NaN, 0xFFFF, 0xFFFFFFFF.
- `codec/cs-decoder.js` decodes types 1, 2, 16 and 17; `codec/test_items.js` (node) checks it with v2
  frames, one with S1000 and Cubic 6303 together, and one v1 frame.
- A v2 uplink has 9 B of header and system data and 4 B of header per device (`[type][addr][status][len]`).

| Device | Items (LoRaWAN encoding) | Data | Device block | Alone in an uplink |
|---|---|---|---|---|
| SenseCAP S1000 | temperature, humidity (F16), pressure (U16 0.1 hPa), wind direction, wind speed (F16), light (F16 klux), rainfall (U32 0.001 mm), PM2.5, PM10, CO2 (F16) | 22 B | 26 B | 35 B |
| SenseCAP S500 | the first five | 10 B | 14 B | 23 B |
| SenseCAP S700 | the first seven, heater temperature, tilt (F16) | 20 B | 24 B | 33 B |
| Cubic 6303 | TSP, PM1.0, PM2.5, PM10, gas flow (F16) | 10 B | 14 B | 23 B |

S1000 and Cubic 6303 fit one uplink (49 B). S1000, S500, S700 and Cubic 6303, in this order, take two
(49 + 47 B); the encoder starts a new uplink at the first device that does not fit.

## 6. New drivers

**SenseCAP S1000 / S700 / S500** (`drv_sensecap.c`)
- FC04, signed 32-bit values with the high word first, / 1000. The S500 uses the first five items of the
  S1000 table and is a new device type, `sensecap_s500` (16; 15 is the Piketronic RPP-U of !881).
- The S700 measures the first seven items plus the heater temperature (0x001C, °C) and the tilt state
  (0x001E, 0 upright, 1 tipped over), both LAST; it is device type `sensecap_s700` (17). Heating itself is
  switched only over the station's service port, not over Modbus.
- Wind direction is CIRCULAR, accumulated rainfall COUNTER, the S700 diagnostics LAST, everything else STATS.
- `device sensecap sample <addr> [s1000|s700|s500]` reads the S1000 table by default.
- `device sensecap config <addr> <new>` writes holding register 0x1000; the station confirms it from the old
  address and applies the new address after a power cycle.

**Cubic 6303** (`drv_cubic_6303.c`)
- FC04: TSP, PM1.0, PM2.5, PM10 as unsigned 32-bit values with the high word first; gas flow as U16 / 100.
  All items STATS.
- The version register is not read.
- `config` writes holding register 2. The sensor answers from the new address, which the Zephyr client
  rejects with -EIO although the change went through.

**Address change** (`app_modbus_addr.c`, `app_modbus_change_addr()`)
- Refuses an address another device answers at (-EADDRINUSE), then writes the address register. Any other
  error than no reply means the bus does not work and is returned as it is (-ENODEV while Modbus is not
  running).
- A confirmed write means the device takes the address after a power cycle; otherwise the register is read
  back at the new address (applied; a second read covers one disturbed frame) and, after a lost or
  unreadable reply, at the old one (stored, power cycle).
- The `config` shell commands print which of the two happened, or why the change failed.

## 7. Integration with !881

**Transport.** Every driver declares its transport (`.transport`, UART or MODBUS). The SC16IS7xx of the X2 /
X12 slot runs one of them per boot: `app_serial` and the Modbus client install the same IRQ callback.
- `app_device_pick_bus()` decides: the transport all configured devices need, or, if they differ, the one
  serial-mode selects (modbus: Modbus, uart and transparent: UART). The driverless types (generic,
  lambrecht) need Modbus, and the UART in transparent mode.
- At boot the bus is initialized first, then `app_device_init()` plans the slots and initializes the
  drivers, so a driver `.init` can already use the bus. A device that needs the other transport, and a
  second device on a UART bus, is not activated (`device list`: not active) instead of sampling into
  `-ENODEV` on every interval.
- This replaces the "first configured device decides" rule of !881. `app config save` already rejects such
  configurations; the boot rule covers `config save` and configurations from the cloud.

**Piketronic RPP-U.** The driver keeps its state in its slot and its samples in the slot's record ring
(`max_instances` 1, the frame has no address). CBOR encodes the records like the RPP-R, with the
`accumulator_voltage` of !881; LoRaWAN sends the latest reading. The low-accumulator alert works as in !881.
`device piketronic_u sample` samples the configured device.

**Piketronic RPP-R.** The slots give every probe its own state and ring, which !881 solved with a context per
address and a shared sample pool; that code is not taken over. A device configured without an address still
polls address 1.

**Buffers.** !881 cleared all sample buffers after every LoRaWAN report and in NONE mode; the slot rings
replace that (§4).

**OR-WE-504.** The register table (§3) replaces the decode helpers that !881 split out of the driver. Their
test vectors moved to `tests/applications/serial/driver_store/src/test_modbus_decode.c`.

**CI.** `scripts/check_telemetry_completeness.py` counts a driver with an item table as encoded when both
encoders call the generic item path.

## Behaviour changes

- Two devices of the same type on different addresses report separately.
- A full buffer drops its oldest entry instead of the newest.
- Reports larger than 8 KB are split into several messages instead of failing every time.
- In LoRaWAN and NONE mode the buffers no longer stay full.
- Shell `device <driver> sample <addr>` no longer adds a sample to the upload buffer.
- `config` of EM1XX, EM5XX, iEM3000, OR-WE-516, Piketronic and SolaX used to set an in-memory sampling address
  that the next scheduled sample overwrote. It now explains that the address is set on the device and in
  `app config device-N`; for EM1XX, EM5XX and iEM3000 also that the parity is set on the meter and in
  `app config serial-parity`.
- `app config save` now saves the configuration and reboots, like `config save`. Before, it ran its checks
  and then only printed the parameter list, so nothing was saved (a bug since the application was added).
  The checks reject a second MicroSENS or Promag device and configurations whose data does not fit;
  `config save` and a configuration from the cloud skip them. On the cloud shell (the dummy shell, which
  also applies cloud configurations line by line) it only validates and says so; `config save` saves there.
- CBOR item values and the BLE tag temperature / humidity are rounded instead of truncated (BLE tag values
  may differ by 0.01).
- `device sensecap sample` takes an optional model; `modbus sample` explains when there is nothing for it to
  sample instead of printing "Sampling complete".
- MicroSENS calibration and reset can no longer overlap a measurement; they wait for it instead of returning
  `-EBUSY`.
- New device types `sensecap_s500` and `sensecap_s700`; SenseCAP S1000 and Cubic 6303 now report data.

Compared with !881:
- LoRaWAN drops the data of a device after the uplink that carried it was sent, and keeps the newest window;
  a failed uplink drops nothing.
- SenseCAP and Cubic 6303 have drivers now, so they need Modbus: `app config save` rejects them in
  transparent mode, and the boot brings them up on Modbus.
- With devices that need both transports, serial-mode picks the bus instead of the first configured device.
- `device piketronic_u sample` needs a configured RPP-U.

## Verification

- **Unit tests** (twister, `native_sim`, run by the GitLab `tests` job): `tests/applications/serial/driver_store`
  (67 cases) and `tests/applications/serial/send_split` (7 cases), next to the frame codec suites of !881
  (MicroSENS 10, Promag 11, RPP-U 12 cases).
- **Codec tests:** `node codec/test_items.js` (61 checks) and `node codec/test_float16.js` (55 checks).
- **CI check:** `scripts/check_telemetry_completeness.py` passes with 15 device types.
- **Builds** in `hardwario/nrf-connect-sdk-build:v3.4.0-1`: every commit builds as the committed tree and as
  the RS-485 variant; the tip also as the RS-232 variant, without compiler warnings. Memory compared with
  !881 at `3a6a42d8`:

| Build | FLASH before → after | RAM before → after |
|---|---|---|
| committed tree | 632 632 → 638 776 B (+6 144) | 229 960 → 227 208 B (−2 752) |
| RS-485 | 658 192 → 663 092 B (+4 900) | 231 688 → 229 000 B (−2 688) |
| RS-232 | 645 460 → 653 880 B (+8 420) | 230 664 → 227 976 B (−2 688) |

  RAM goes down because the slot pool replaces the static buffers of the RPP-R and RPP-U drivers.

- `west chester-update --variant ...` leaves the edits in `app_config.c/.h`, `app_shell.c` and the preserved
  `cmake` block of `CMakeLists.txt` intact.

- **Hardware:** not tested on hardware yet. Console checklist for a CHESTER Serial RS-485 with stations:
  1. `app config serial-mode modbus`, `app config mode lte`, `app config device-0 sensecap_s1000,1`,
     `app config device-1 sensecap_s1000,2`, `app config interval-aggreg 60`, `app config save` (validates,
     saves and reboots; `config show` afterwards shows the values).
  2. `device list` (ring sizes), `sample`, `device sample 0`, `device sample 1` (separate values).
  3. After two minutes `device sample 0` shows a closed window; `send` and check the decoded message.
  4. `app config mode lrw`, save, `send`: two uplinks of 35 B (two S1000 do not fit one), decoded by
     `cs-decoder.js`. With a Cubic 6303 instead of the second S1000: one uplink of 49 B.
  5. Two energy meters of one type at addresses 1 and 2: each slot reports its own `data` list.
  6. Modbus mode with a device without an address, `app config save`: an error and no reboot.
  7. `device em1xx config 1 2 8E1`: guidance, no error. `device sensecap sample 1 s500`: five items titled
     S500. `modbus sample` with only driver devices: the hint.
  8. Aggregation with `interval-aggreg 60` while an LTE send takes longer than 30 s: no empty window
     appears and the window timestamps stay one minute apart.
  9. `device cubic_6303 config 1 5`: "Address changed from 1 to 5", `device cubic_6303 sample 5` reads.
     `device sensecap config <addr> <new>`: "stored, power-cycle"; after the power cycle the station reads
     at the new address. A new address taken by the other device: "already used on the bus". Before the
     power cycle, `modbus read <old> 0x1000 1 holding` shows whether the station reports the new address
     already. With Modbus not running: "Modbus is not running".
  10. `device-0 sensecap_s700,<addr>`: nine items with `heater_temperature` and `tilt`; the LoRaWAN uplink is
      33 B and `cs-decoder.js` shows both; the CBOR message has them as `value`.

## Follow-ups (not in this change)

- OR-WE-504 LoRaWAN divides `energy_in` by 1000 although the driver already stores kWh (`app_lrw.c`).
- The byte counts in the `app_lrw.c` comments are wrong for EM5XX / OR-WE-516 / FlowT.
- MicroSENS LoRaWAN encodes CO2 × 10 and loses 0.04 % steps.
- Remaining drivers could move to item tables (power as STATS, energy as COUNTER), which changes their
  wire format.
- Meteo M keeps its own copy of the SenseCAP / Cubic code.
- The STATS median uses the first `samples_cap` (at most 32) samples of a window; with
  `interval-aggreg / interval-sample` > 32 or extra `device sample <idx>` calls it is not the window median.
  Keeping every k-th sample would fix it.
- The split re-encodes a part when it sends it; the LTE connection parameters in the header are read live,
  so the send can exceed the size the part was planned with. The split then stops with `-ENOSPC` and the
  rest of the report is dropped.
- `message.sequence` increments with every send attempt, so a message delivered later from the cloud spool
  keeps a unique number. It restarts at 0 after a reboot, as in the other applications.
- A Float16 overflow (above 65519, e.g. Cubic TSP) decodes as ±Infinity, not null.
- BLE tag aggregation appends one measurement for ticks that coalesced, so later measurements are labelled
  too early; the device windows count the timer expirations, the BLE tags could do the same.
- When ticks coalesced, the samples of the open window (taken before the work queue was blocked) go into
  the newest window and the inserted empty windows come before it, so they are stamped up to the blocked
  time too late. Putting them first would leave LoRaWAN, which sends the newest window, with an empty one.
- The timeline anchors at the time the window close ran (after a report, or a set clock). A close that ran
  late behind the send stamps the next report period late by that delay (bounded, not accumulating);
  `k_timer_expires_ticks()` could give the nominal tick time.
- `app_device_config()` has no callers (before this change too).
