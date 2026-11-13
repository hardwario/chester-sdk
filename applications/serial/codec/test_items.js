/**
 * LoRaWAN decoding tests for table-driven devices (SenseCAP S1000 / S500 / S700, Cubic 6303).
 *
 * The frames are built byte by byte as app_lrw.c encodes them (payload v2, encode_items):
 * header with the device count, system data (8 B), then per device a header
 * [type][addr][status][len] (4 B) and one value per item.
 *
 * Run: node test_items.js
 */

var decoder = require("./cs-decoder.js");

var passed = 0;
var failed = 0;

function check(name, actual, expected, tolerance) {
  var ok;

  if (expected === null) {
    ok = actual === null;
  } else {
    ok = actual !== null && Math.abs(actual - expected) <= (tolerance || 0);
  }

  if (ok) {
    passed++;
  } else {
    failed++;
    console.log("FAIL " + name + ": got " + actual + ", expected " + expected);
  }
}

/* Float16 bit patterns of values exactly representable in Float16 */
var F16 = {
  1: 0x3c00,
  21.5: 0x4d60,
  45.25: 0x51a8,
  350: 0x5d78,
  3.5: 0x4300,
  54.3125: 0x52ca,
  12: 0x4a00,
  20: 0x4d00,
  415: 0x5e7c,
  NaN: 0x7e00
};

function data(values) {
  var bytes = [];

  values.forEach(function (v) {
    for (var i = 0; i < v.size; i++) {
      bytes.push((v.raw >>> (8 * i)) & 0xff);
    }
  });

  return bytes;
}

/* Device block: [type][addr][status][len], then the data */
function block(type, values) {
  var bytes = data(values);

  return [type, 1, 0x01, bytes.length].concat(bytes);
}

/* v2 message: header (version 2, device count), system data, device blocks */
function message(blocks) {
  var bytes = [0x20 | blocks.length, 0, 0, 0, 0, 0, 0, 0, 0];

  blocks.forEach(function (b) {
    bytes = bytes.concat(b);
  });

  return bytes;
}

function frame(type, values) {
  return message([block(type, values)]);
}

function decode(bytes) {
  return decoder.decodeUplink({ bytes: bytes }).data;
}

function f16(value) {
  return { size: 2, raw: F16[value] };
}

function u16(raw) {
  return { size: 2, raw: raw };
}

function u32(raw) {
  return { size: 4, raw: raw };
}

/* S1000, all values present */
var s1000 = frame(1, [
  f16(21.5), // air_temperature
  f16(45.25), // air_humidity
  u16(10133), // barometric_pressure, 0.1 hPa
  f16(350), // wind_direction_avg
  f16(3.5), // wind_speed_avg
  f16(54.3125), // light_intensity, klux
  u32(12345), // accumulated_rainfall, 0.001 mm
  f16(12), // pm2_5
  f16(20), // pm10
  f16(415) // co2
]);

check("s1000 frame length", s1000.length, 35);

var d = decode(s1000).devices[0];

check("s1000 type", d.type, 1);
check("s1000 air_temperature", d.data.air_temperature, 21.5);
check("s1000 air_humidity", d.data.air_humidity, 45.25);
check("s1000 barometric_pressure", d.data.barometric_pressure, 101330);
check("s1000 wind_direction_avg", d.data.wind_direction_avg, 350);
check("s1000 wind_speed_avg", d.data.wind_speed_avg, 3.5);
check("s1000 light_intensity", d.data.light_intensity, 54312.5);
check("s1000 accumulated_rainfall", d.data.accumulated_rainfall, 12.345, 1e-9);
check("s1000 pm2_5", d.data.pm2_5, 12);
check("s1000 pm10", d.data.pm10, 20);
check("s1000 co2", d.data.co2, 415);
check("s1000 name", d.type_name === "sensecap_s1000" ? 1 : 0, 1);

/* S500, unavailable values */
var s500 = frame(16, [f16(NaN), f16(45.25), u16(0xffff), f16(NaN), f16(3.5)]);

check("s500 frame length", s500.length, 23);

d = decode(s500).devices[0];

check("s500 type", d.type, 16);
check("s500 name", d.type_name === "sensecap_s500" ? 1 : 0, 1);
check("s500 air_temperature", d.data.air_temperature, null);
check("s500 air_humidity", d.data.air_humidity, 45.25);
check("s500 barometric_pressure", d.data.barometric_pressure, null);
check("s500 wind_direction_avg", d.data.wind_direction_avg, null);
check("s500 wind_speed_avg", d.data.wind_speed_avg, 3.5);
check("s500 has no light_intensity", d.data.light_intensity === undefined ? 1 : 0, 1);

/* Type 15 is the Piketronic RPP-U, not the S500; an accumulator voltage of 0 is not reported */
var type15 = frame(15, [f16(20), f16(12), f16(21.5), f16(45.25), u16(0)]);

d = decode(type15).devices[0];

check("type 15 is not s500", d.type_name === "sensecap_s500" ? 1 : 0, 0);
check("type 15 name", d.type_name === "piketronic_u" ? 1 : 0, 1);
check("rpp-u radon_concentration", d.data.radon_concentration, 20);
check("rpp-u accumulator_voltage not reported", d.data.accumulator_voltage, null);

d = decode(frame(15, [f16(20), f16(12), f16(21.5), f16(45.25), f16(3.5)])).devices[0];

check("rpp-u accumulator_voltage", d.data.accumulator_voltage, 3.5);

/* S700, all values present, the station tipped over */
var s700 = frame(17, [
  f16(21.5), // air_temperature
  f16(45.25), // air_humidity
  u16(10133), // barometric_pressure, 0.1 hPa
  f16(350), // wind_direction_avg
  f16(3.5), // wind_speed_avg
  f16(54.3125), // light_intensity, klux
  u32(12345), // accumulated_rainfall, 0.001 mm
  f16(12), // heater_temperature
  f16(1) // tilt
]);

check("s700 frame length", s700.length, 33);

d = decode(s700).devices[0];

check("s700 type", d.type, 17);
check("s700 name", d.type_name === "sensecap_s700" ? 1 : 0, 1);
check("s700 air_temperature", d.data.air_temperature, 21.5);
check("s700 air_humidity", d.data.air_humidity, 45.25);
check("s700 barometric_pressure", d.data.barometric_pressure, 101330);
check("s700 wind_direction_avg", d.data.wind_direction_avg, 350);
check("s700 wind_speed_avg", d.data.wind_speed_avg, 3.5);
check("s700 light_intensity", d.data.light_intensity, 54312.5);
check("s700 accumulated_rainfall", d.data.accumulated_rainfall, 12.345, 1e-9);
check("s700 heater_temperature", d.data.heater_temperature, 12);
check("s700 tilt", d.data.tilt, 1);
check("s700 has no pm2_5", d.data.pm2_5 === undefined ? 1 : 0, 1);

/* S700, diagnostics unavailable */
var s700na = frame(17, [
  f16(21.5),
  f16(45.25),
  u16(10133),
  f16(350),
  f16(3.5),
  f16(54.3125),
  u32(12345),
  f16(NaN),
  f16(NaN)
]);

d = decode(s700na).devices[0];

check("s700 heater_temperature unavailable", d.data.heater_temperature, null);
check("s700 tilt unavailable", d.data.tilt, null);

/* Counter not available */
var rain = frame(1, [
  f16(21.5),
  f16(45.25),
  u16(10133),
  f16(350),
  f16(3.5),
  f16(54.3125),
  u32(0xffffffff),
  f16(12),
  f16(20),
  f16(415)
]);

d = decode(rain).devices[0];

check("rainfall unavailable", d.data.accumulated_rainfall, null);

/* A type the decoder does not know is skipped by its length */
var unknown = message([
  block(99, [u32(0xdeadbeef)]),
  block(16, [f16(NaN), f16(45.25), u16(0xffff), f16(NaN), f16(3.5)])
]);

var m = decode(unknown);

check("unknown type name", m.devices[0].type_name === "unknown" ? 1 : 0, 1);
check("after unknown type", m.devices[1].data.air_humidity, 45.25);

/* A block longer than the payload */
var cut = frame(16, [f16(NaN), f16(45.25), u16(0xffff), f16(NaN), f16(3.5)]).slice(0, 18);

m = decode(cut);

check("truncated block", m.devices[0].data.error === "device data truncated" ? 1 : 0, 1);

/* Payload v1 (one device, 3-byte device header) still decodes */
var v1 = [0x10, 0, 0, 0, 0, 0, 0, 0, 0, 16, 1, 0x01].concat(
  data([f16(NaN), f16(45.25), u16(0xffff), f16(NaN), f16(3.5)])
);

d = decode(v1).device;

check("v1 s500 type", d.type, 16);
check("v1 s500 air_humidity", d.data.air_humidity, 45.25);

/* Cubic 6303 */
var cubic = frame(2, [f16(20), f16(12), f16(12), f16(20), f16(3.5)]);

check("cubic frame length", cubic.length, 23);

d = decode(cubic).devices[0];

check("cubic type", d.type, 2);
check("cubic tsp", d.data.tsp, 20);
check("cubic pm1_0", d.data.pm1_0, 12);
check("cubic pm2_5", d.data.pm2_5, 12);
check("cubic pm10", d.data.pm10, 20);
check("cubic gas_flow", d.data.gas_flow, 3.5);

/* S1000 and Cubic 6303 in one uplink */
var pair = message([
  block(1, [
    f16(21.5),
    f16(45.25),
    u16(10133),
    f16(350),
    f16(3.5),
    f16(54.3125),
    u32(12345),
    f16(12),
    f16(20),
    f16(415)
  ]),
  block(2, [f16(20), f16(12), f16(12), f16(20), f16(3.5)])
]);

check("s1000 + cubic frame length", pair.length, 49);

m = decode(pair);

check("s1000 + cubic device_count", m.device_count, 2);
check("s1000 + cubic first type", m.devices[0].type, 1);
check("s1000 + cubic co2", m.devices[0].data.co2, 415);
check("s1000 + cubic second type", m.devices[1].type, 2);
check("s1000 + cubic gas_flow", m.devices[1].data.gas_flow, 3.5);

console.log(passed + " passed, " + failed + " failed");
process.exit(failed ? 1 : 0);
