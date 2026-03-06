/**
 * Float16 encode/decode roundtrip tests for CHESTER Serial LRW codec.
 *
 * Tests the JavaScript decoder (float16_to_float32) against known Float16
 * bit patterns and validates the C encoder behavior by simulating
 * float32_to_float16 in JavaScript.
 *
 * Run: node test_float16.js
 */

var decoder = require("./cs-decoder.js");

var passed = 0;
var failed = 0;

// ============================================================================
// Reference float32_to_float16 (mirrors the C implementation in app_lrw.c)
// ============================================================================

function float32_to_float16(value) {
  if (isNaN(value)) {
    return 0x7E00;
  }

  var buf = new ArrayBuffer(4);
  var view = new DataView(buf);
  view.setFloat32(0, value, false);
  var f32 = view.getUint32(0, false);

  var sign = (f32 >>> 16) & 0x8000;

  if (!isFinite(value)) {
    return sign | 0x7C00;
  }

  var exp = ((f32 >>> 23) & 0xFF) - 127;
  var mantissa = f32 & 0x007FFFFF;
  var f16_exp = exp + 15;

  // Overflow
  if (f16_exp >= 31) {
    return sign | 0x7C00;
  }

  // Subnormal
  if (f16_exp <= 0) {
    if (f16_exp < -10) {
      return sign;
    }
    mantissa |= 0x00800000;
    var shift = 14 - f16_exp;
    var round_bit = 1 << (shift - 1);
    var sticky = mantissa & (round_bit - 1);
    var m = mantissa >>> shift;
    if ((mantissa & round_bit) && (sticky || (m & 1))) {
      m++;
    }
    return sign | (m & 0x3FF);
  }

  // Normal
  var m = (mantissa >>> 13) & 0x3FF;
  var round_bit = mantissa & 0x1000;
  var sticky = mantissa & 0x0FFF;
  if (round_bit && (sticky || (m & 1))) {
    m++;
    if (m > 0x3FF) {
      m = 0;
      f16_exp++;
      if (f16_exp >= 31) {
        return sign | 0x7C00;
      }
    }
  }

  return sign | ((f16_exp & 0x1F) << 10) | m;
}

// ============================================================================
// Test helpers
// ============================================================================

function f16_to_bytes(f16) {
  return [f16 & 0xFF, (f16 >> 8) & 0xFF]; // Little-endian
}

function assert_decode(name, f16_value, expected) {
  var bytes = f16_to_bytes(f16_value);
  var result = decoder.float16_to_float32(bytes, 0);

  var ok = false;
  if (expected === null || expected === undefined) {
    ok = isNaN(result);
  } else if (isNaN(expected)) {
    ok = isNaN(result);
  } else if (expected === 0 && 1 / expected === -Infinity) {
    ok = result === 0 && 1 / result === -Infinity; // -0
  } else if (!isFinite(expected)) {
    ok = result === expected;
  } else {
    ok = Math.abs(result - expected) < 1e-10;
  }

  if (ok) {
    passed++;
  } else {
    failed++;
    console.log("FAIL [decode] " + name + ": f16=0x" +
      f16_value.toString(16).toUpperCase().padStart(4, "0") +
      " expected=" + expected + " got=" + result);
  }
}

function assert_encode(name, float_value, expected_f16) {
  var result = float32_to_float16(float_value);

  if (result === expected_f16) {
    passed++;
  } else {
    failed++;
    console.log("FAIL [encode] " + name + ": input=" + float_value +
      " expected=0x" + expected_f16.toString(16).toUpperCase().padStart(4, "0") +
      " got=0x" + result.toString(16).toUpperCase().padStart(4, "0"));
  }
}

function assert_roundtrip(name, float_value, tolerance) {
  var f16 = float32_to_float16(float_value);
  var bytes = f16_to_bytes(f16);
  var result = decoder.float16_to_float32(bytes, 0);

  var ok = false;
  if (isNaN(float_value)) {
    ok = isNaN(result);
  } else if (!isFinite(float_value)) {
    ok = result === float_value;
  } else if (float_value === 0) {
    ok = result === 0;
  } else {
    var rel_error = Math.abs((result - float_value) / float_value);
    ok = rel_error <= (tolerance || 0.001); // 0.1% default tolerance
  }

  if (ok) {
    passed++;
  } else {
    failed++;
    console.log("FAIL [roundtrip] " + name + ": input=" + float_value +
      " f16=0x" + f16.toString(16).toUpperCase().padStart(4, "0") +
      " decoded=" + result);
  }
}

// ============================================================================
// Tests: Decoder (float16_to_float32)
// ============================================================================

console.log("=== Decoder tests ===");

// Special values
assert_decode("NaN sentinel", 0x7E00, NaN);
assert_decode("+Infinity", 0x7C00, Infinity);
assert_decode("-Infinity", 0xFC00, -Infinity);
assert_decode("+Zero", 0x0000, 0);
assert_decode("-Zero", 0x8000, -0);
assert_decode("NaN (other)", 0x7C01, NaN);

// Normal values
assert_decode("1.0", 0x3C00, 1.0);
assert_decode("-1.0", 0xBC00, -1.0);
assert_decode("0.5", 0x3800, 0.5);
assert_decode("-0.5", 0xB800, -0.5);
assert_decode("2.0", 0x4000, 2.0);
assert_decode("max (65504)", 0x7BFF, 65504.0);
assert_decode("-max (-65504)", 0xFBFF, -65504.0);
assert_decode("min normal (2^-14)", 0x0400, Math.pow(2, -14));

// Subnormal values
assert_decode("min subnormal", 0x0001, Math.pow(2, -14) / 1024.0);
assert_decode("subnormal 0x0200", 0x0200, Math.pow(2, -14) * 0.5);
assert_decode("subnormal 0x03FF", 0x03FF, Math.pow(2, -14) * (1023.0 / 1024.0));
assert_decode("-min subnormal", 0x8001, -Math.pow(2, -14) / 1024.0);

// ============================================================================
// Tests: Encoder (float32_to_float16)
// ============================================================================

console.log("=== Encoder tests ===");

// Special values
assert_encode("NaN", NaN, 0x7E00);
assert_encode("+Infinity", Infinity, 0x7C00);
assert_encode("-Infinity", -Infinity, 0xFC00);
assert_encode("+0", 0.0, 0x0000);
assert_encode("-0", -0.0, 0x8000);

// Normal values
assert_encode("1.0", 1.0, 0x3C00);
assert_encode("-1.0", -1.0, 0xBC00);
assert_encode("0.5", 0.5, 0x3800);
assert_encode("2.0", 2.0, 0x4000);
assert_encode("65504.0 (max)", 65504.0, 0x7BFF);

// Overflow
assert_encode("70000 -> +Inf", 70000.0, 0x7C00);
assert_encode("-70000 -> -Inf", -70000.0, 0xFC00);
assert_encode("100000 -> +Inf", 100000.0, 0x7C00);

// Underflow (too small for subnormal)
assert_encode("1e-8 -> 0", 1e-8, 0x0000);
assert_encode("-1e-8 -> -0", -1e-8, 0x8000);

// Rounding: 0.1 should be 0x2E66 (round-to-nearest-even)
// 0.1 in f32: exp=-4 (f16_exp=11), mantissa=0x4CCCCD
// mantissa >> 13 = 0x266, bit12 = 0x800 & 0x1000 = 0x1000, sticky = 0x0CCD
// round up: 0x267 -> but wait, let me compute correctly
// Actually: 0.1f = 0x3DCCCCCD in IEEE754
// exp = 0x7B - 127 = -4, f16_exp = 11
// mantissa = 0x4CCCCD, mantissa>>13 = 0x266, round_bit = (0x4CCCCD & 0x1000) = 0x1000
// sticky = 0x4CCCCD & 0x0FFF = 0xCCD (nonzero)
// so round up: m = 0x267
// f16 = 0 | (11 << 10) | 0x267 = 0x2E67
assert_encode("0.1 (rounding)", 0.1, 0x2E66);

// ============================================================================
// Tests: Roundtrip (encode -> decode)
// ============================================================================

console.log("=== Roundtrip tests ===");

// Typical sensor values
assert_roundtrip("voltage 230V", 230.0, 0.002);
assert_roundtrip("voltage 400V", 400.0, 0.002);
assert_roundtrip("current 0.5A", 0.5, 0.002);
assert_roundtrip("current 16A", 16.0, 0.002);
assert_roundtrip("current 100A", 100.0, 0.002);
assert_roundtrip("power 1000W", 1000.0, 0.002);
assert_roundtrip("power 10000W", 10000.0, 0.002);
assert_roundtrip("power -500W", -500.0, 0.002);
assert_roundtrip("frequency 50Hz", 50.0, 0.002);
assert_roundtrip("energy 45000Wh", 45000.0, 0.002);
assert_roundtrip("small current 0.01A", 0.01, 0.002);
assert_roundtrip("temperature 22.5C", 22.5, 0.002);
assert_roundtrip("CO2 0.04%", 0.04, 0.01);
assert_roundtrip("pressure 1013", 1013.0, 0.002);

// Special roundtrips
assert_roundtrip("NaN", NaN);
assert_roundtrip("+Infinity", Infinity);
assert_roundtrip("-Infinity", -Infinity);
assert_roundtrip("zero", 0.0);

// Edge cases
assert_roundtrip("max f16", 65504.0, 0.001);
assert_roundtrip("near max", 65000.0, 0.002);
assert_roundtrip("small positive", 0.0001, 0.01);

// ============================================================================
// Results
// ============================================================================

console.log("\n=== Results ===");
console.log("Passed: " + passed);
console.log("Failed: " + failed);

if (failed > 0) {
  process.exit(1);
} else {
  console.log("All tests passed!");
  process.exit(0);
}
