/**
 * CHESTER Serial - LoRaWAN Payload Decoder (Float16)
 *
 * Supports:
 * - ChirpStack v4 (decodeUplink)
 * - ChirpStack v3 (Decode)
 * - The Things Network (Decoder)
 *
 * Payload structure:
 * - Header (1 byte): [version:4bit][device_index:4bit]
 * - System data: Battery (5B), Accelerometer (1B), Thermometer (2B)
 * - Device: [type:u8][addr:u8][status:u8][data:...]
 *   - Single-phase (EM1XX, OR-WE-504): 12 bytes (6x Float16)
 *   - Three-phase (EM5XX, OR-WE-516): 30 bytes (15x Float16)
 *   - Three-phase (iEM3000): 26 bytes (13x Float16)
 *   - MicroSENS 180-HS: 6 bytes (3x INT16)
 *   - Promag MF7S: 10 bytes (4B UID + 4B timestamp + 2B counter)
 *   - FlowT FT201: 13 bytes (6x Float16 + 1B signal quality)
 */

var cursor = 0;
var buffer;

// Device type constants (must match app_config.h)
var DEVICE_TYPE_NONE = 0;
var DEVICE_TYPE_SENSECAP_S1000 = 1;
var DEVICE_TYPE_CUBIC_6303 = 2;
var DEVICE_TYPE_LAMBRECHT = 3;
var DEVICE_TYPE_GENERIC = 4;
var DEVICE_TYPE_MICROSENS_180HS = 5;
var DEVICE_TYPE_OR_WE_504 = 6;
var DEVICE_TYPE_EM1XX = 7;
var DEVICE_TYPE_OR_WE_516 = 8;
var DEVICE_TYPE_EM5XX = 9;
var DEVICE_TYPE_IEM3000 = 10;
var DEVICE_TYPE_PROMAG_MF7S = 11;
var DEVICE_TYPE_FLOWT_FT201 = 12;

// ChirpStack v4
function decodeUplink(input) {
  cursor = 0;
  buffer = input.bytes || input;

  var data = {};

  // Parse header: [version:4bit][device_index:4bit]
  var header = u8();
  data.version = (header >> 4) & 0x0F;
  data.device_index = header & 0x0F;

  // System data (always present)
  decode_batt(data);
  decode_accel(data);
  decode_therm(data);

  // Single device data
  var device = decode_serial_device(data.version);
  data.device = device;

  return { data: data };
}

// The Things Network
function Decoder(bytes, port) {
  var decoded = decodeUplink({ bytes: bytes, fPort: port });
  return decoded.data;
}

// ChirpStack v3
function Decode(fPort, bytes) {
  var decoded = decodeUplink({ fPort: fPort, bytes: bytes });
  return decoded.data;
}

// ============================================================================
// System data decoders
// ============================================================================

function decode_batt(data) {
  data.voltage_rest = u16();
  data.voltage_load = u16();
  data.current_load = u8();

  if (data.voltage_rest === 0xffff) {
    data.voltage_rest = null;
  } else {
    data.voltage_rest = data.voltage_rest / 1000;
  }

  if (data.voltage_load === 0xffff) {
    data.voltage_load = null;
  } else {
    data.voltage_load = data.voltage_load / 1000;
  }

  if (data.current_load === 0xff) {
    data.current_load = null;
  }
}

function decode_accel(data) {
  data.orientation = u8();

  if (data.orientation === 0xff) {
    data.orientation = null;
  }
}

function decode_therm(data) {
  data.therm_temperature = s16();

  if (data.therm_temperature === 0x7fff) {
    data.therm_temperature = null;
  } else {
    data.therm_temperature /= 100;
  }
}

// ============================================================================
// Serial device decoders
// ============================================================================

function decode_serial_device(version) {
  var device = {};

  // Device header (3 bytes): [type:u8][addr:u8][status:u8]
  device.type = u8();
  device.addr = u8();
  device.status = u8();
  device.valid = (device.status & 0x01) !== 0;
  device.error = (device.status & 0x02) !== 0;

  if (device.type === DEVICE_TYPE_NONE) {
    return device;
  }

  // Decode device-specific data based on type and protocol version
  device.data = {};
  switch (device.type) {
    case DEVICE_TYPE_MICROSENS_180HS:
      device.type_name = "microsens_180hs";
      decode_microsens(device.data); // Always INT16
      break;
    case DEVICE_TYPE_EM1XX:
      device.type_name = "em1xx";
      if (version === 1) {
        decode_em1xx_float16(device.data);
      } else {
        decode_em1xx(device.data); // Legacy INT16
      }
      break;
    case DEVICE_TYPE_EM5XX:
      device.type_name = "em5xx";
      if (version === 1) {
        decode_em5xx_float16(device.data);
      } else {
        decode_em5xx(device.data); // Legacy INT16
      }
      break;
    case DEVICE_TYPE_OR_WE_504:
      device.type_name = "or_we_504";
      if (version === 1) {
        decode_or_we_504_float16(device.data);
      } else {
        decode_or_we_504(device.data); // Legacy INT16
      }
      break;
    case DEVICE_TYPE_OR_WE_516:
      device.type_name = "or_we_516";
      if (version === 1) {
        decode_or_we_516_float16(device.data);
      } else {
        decode_or_we_516(device.data); // Legacy INT16
      }
      break;
    case DEVICE_TYPE_IEM3000:
      device.type_name = "iem3000";
      if (version === 1) {
        decode_iem3000_float16(device.data);
      } else {
        device.data.error = "Legacy INT16 format not supported";
      }
      break;
    case DEVICE_TYPE_PROMAG_MF7S:
      device.type_name = "promag_mf7s";
      decode_promag_mf7s(device.data);
      break;
    case DEVICE_TYPE_FLOWT_FT201:
      device.type_name = "flowt_ft201";
      if (version === 1) {
        decode_flowt_ft201_float16(device.data);
      } else {
        device.data.error = "Legacy format not supported";
      }
      break;
    default:
      device.type_name = "unknown";
      break;
  }

  return device;
}

/**
 * MicroSENS 180-HS CO2 sensor decoder
 *
 * Data format (6 bytes):
 * - co2_percent: s16, Vol.-% * 10 (0x7FFF = invalid)
 * - temperature: s16, C * 10 (0x7FFF = invalid)
 * - pressure: u16, mbar (0xFFFF = invalid)
 */
function decode_microsens(data) {
  // CO2 concentration (Vol.-%)
  var co2_raw = s16();
  if (co2_raw === 0x7fff) {
    data.co2_percent = null;
  } else {
    data.co2_percent = co2_raw / 10.0;
  }

  // Temperature (Celsius)
  var temp_raw = s16();
  if (temp_raw === 0x7fff) {
    data.temperature = null;
  } else {
    data.temperature = temp_raw / 10.0;
  }

  // Pressure (mbar)
  var pressure_raw = u16();
  if (pressure_raw === 0xffff) {
    data.pressure = null;
  } else {
    data.pressure = pressure_raw;
  }
}

// ============================================================================
// Float16 (IEEE 754-2008) Conversion
// ============================================================================

/**
 * Convert Float16 to Float32
 *
 * Handles: normal, subnormal, zero, infinity, NaN sentinel (0x7E00).
 *
 * @param {Uint8Array} bytes - Buffer containing data
 * @param {number} offset - Offset to read from
 * @return {number} Float32 value (or NaN for sentinel)
 */
function float16_to_float32(bytes, offset) {
  var f16 = bytes[offset] | (bytes[offset + 1] << 8);

  // Check for NaN sentinel
  if (f16 === 0x7E00) {
    return NaN;
  }

  // Extract components
  var sign = (f16 & 0x8000) >> 15;
  var exp = (f16 & 0x7C00) >> 10;
  var mantissa = f16 & 0x03FF;

  // Handle special cases
  if (exp === 0) {
    if (mantissa === 0) {
      return sign ? -0 : 0;
    }
    // Subnormal: value = (-1)^sign * 2^(-14) * (mantissa / 1024)
    var subnormal = (mantissa / 1024.0) * Math.pow(2, -14);
    return sign ? -subnormal : subnormal;
  }

  if (exp === 31) {
    if (mantissa !== 0) {
      return NaN; // NaN (other than sentinel)
    }
    return sign ? -Infinity : Infinity;
  }

  // Rebias exponent from float16 (bias=15) to float32 (bias=127)
  exp = exp - 15 + 127;

  // Shift mantissa to float32 position (10 bits -> 23 bits)
  mantissa = mantissa << 13;

  // Construct float32 bit pattern
  var f32_bits = (sign << 31) | (exp << 23) | mantissa;

  // Convert bits to float using ArrayBuffer
  var buf = new ArrayBuffer(4);
  var view = new DataView(buf);
  view.setUint32(0, f32_bits, false); // Big-endian
  return view.getFloat32(0, false);
}

/**
 * Legacy INT16 decoders (for backward compatibility)
 */
function decode_em1xx(data) {
  // Legacy v0 format - not implemented, use Float16
  data.error = "Legacy INT16 format not supported";
}

function decode_em5xx(data) {
  // Legacy v0 format - not implemented, use Float16
  data.error = "Legacy INT16 format not supported";
}

function decode_or_we_504(data) {
  // Legacy v0 format - not implemented, use Float16
  data.error = "Legacy INT16 format not supported";
}

function decode_or_we_516(data) {
  // Legacy v0 format - not implemented, use Float16
  data.error = "Legacy INT16 format not supported";
}

// ============================================================================
// Float16 decoders (Protocol version 1)
// ============================================================================

/**
 * EM1XX Float16 decoder (6x Float16 = 12 bytes)
 * Full dataset: voltage, current, power, frequency, energy_in, energy_out
 */
function decode_em1xx_float16(data) {
  data.voltage = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.frequency = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_in = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_out = float16_to_float32(buffer, cursor);
  cursor += 2;
}

/**
 * EM5XX Float16 decoder (15x Float16 = 30 bytes)
 * Full three-phase dataset
 */
function decode_em5xx_float16(data) {
  // Per-phase voltages
  data.voltage_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase currents
  data.current_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase powers
  data.power_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Totals
  data.power_total = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.frequency = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_in = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_out = float16_to_float32(buffer, cursor);
  cursor += 2;
}

/**
 * OR-WE-504 Float16 decoder (6x Float16 = 12 bytes)
 * Same as EM1XX
 */
function decode_or_we_504_float16(data) {
  data.voltage = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.frequency = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_in = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_out = float16_to_float32(buffer, cursor);
  cursor += 2;
}

/**
 * OR-WE-516 Float16 decoder (15x Float16 = 30 bytes)
 * Same as EM5XX
 */
function decode_or_we_516_float16(data) {
  // Per-phase voltages
  data.voltage_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase currents
  data.current_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase powers
  data.power_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Totals
  data.power_total = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.frequency = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_out = float16_to_float32(buffer, cursor);
  cursor += 2;
}

/**
 * iEM3000 Float16 decoder (13x Float16 = 26 bytes)
 * Same layout as EM5XX
 */
function decode_iem3000_float16(data) {
  // Per-phase voltages
  data.voltage_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.voltage_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase currents
  data.current_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.current_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Per-phase powers
  data.power_l1 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l2 = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.power_l3 = float16_to_float32(buffer, cursor);
  cursor += 2;

  // Totals
  data.power_total = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.frequency = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_in = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_out = float16_to_float32(buffer, cursor);
  cursor += 2;
}

/**
 * Promag MF7S decoder (4B UID + 4B timestamp + 2B counter = 10 bytes)
 */
function decode_promag_mf7s(data) {
  // UID (4 bytes, big-endian)
  var b0 = u8();
  var b1 = u8();
  var b2 = u8();
  var b3 = u8();
  data.uid = ((b0 << 24) | (b1 << 16) | (b2 << 8) | b3) >>> 0;

  if (data.uid === 0) {
    data.uid = null;
  } else {
    data.uid_hex = ("00000000" + data.uid.toString(16).toUpperCase()).slice(-8);
  }

  // Timestamp (4 bytes, little-endian, Unix epoch)
  data.timestamp = u32();
  if (data.timestamp === 0) {
    data.timestamp = null;
  }

  // Total reads counter (2 bytes, little-endian)
  data.total_reads = u16();
}

/**
 * FlowT FT201 Float16 decoder (6×Float16 + 1B = 13 bytes)
 * flow_h, velocity, temp_inlet, temp_outlet, energy_hot, energy_cold, signal_quality
 */
function decode_flowt_ft201_float16(data) {
  data.flow_h = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.velocity = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.temp_inlet = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.temp_outlet = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_hot = float16_to_float32(buffer, cursor);
  cursor += 2;
  data.energy_cold = float16_to_float32(buffer, cursor);
  cursor += 2;

  data.signal_quality = u8();
  if (data.signal_quality === 0xFF) {
    data.signal_quality = null;
  }
}

// ============================================================================
// Helper functions
// ============================================================================

function BIT(index) {
  return 1 << index;
}

function s8() {
  var value = buffer.slice(cursor)[0];
  cursor += 1;
  return value > 0x7f ? value - 0x100 : value;
}

function u8() {
  var value = buffer.slice(cursor)[0];
  cursor += 1;
  return value;
}

function s16() {
  var value = buffer.slice(cursor);
  value = value[0] | (value[1] << 8);
  cursor += 2;
  return value > 0x7fff ? value - 0x10000 : value;
}

function u16() {
  var value = buffer.slice(cursor);
  value = value[0] | (value[1] << 8);
  cursor += 2;
  return value;
}

function u32() {
  var value = buffer.slice(cursor);
  value = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);
  cursor += 4;
  return value >>> 0;
}

function s32() {
  var value = buffer.slice(cursor);
  value = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);
  cursor += 4;
  return value;
}

// Used for testing
function setBuffer(buf) {
  buffer = buf;
  cursor = 0;
}

if (typeof module !== "undefined") {
  module.exports = {
    decodeUplink: decodeUplink,
    Decoder: Decoder,
    Decode: Decode,
    decode_iem3000_float16: decode_iem3000_float16,
    decode_promag_mf7s: decode_promag_mf7s,
    decode_flowt_ft201_float16: decode_flowt_ft201_float16,
    float16_to_float32: float16_to_float32,
    s8: s8,
    u8: u8,
    s16: s16,
    u16: u16,
    s32: s32,
    u32: u32,
    setBuffer: setBuffer
  };
}
