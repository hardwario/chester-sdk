/*

See README.md for usage instructions

*/

// Include the decoder module
const decoder = require('../cs-decoder.js');

// s8 tests
decoder.setBuffer(Buffer.from([ 0x7f ]));
assert(decoder.s8() === 127, "s8 positive max");

decoder.setBuffer(Buffer.from([ 0x80 ]));
assert(decoder.s8() === -128, "s8 negative max");

decoder.setBuffer(Buffer.from([ 0x00 ]));
assert(decoder.s8() === 0, "s8 zero");

// u8 tests
decoder.setBuffer(Buffer.from([ 0xff ]));
assert(decoder.u8() === 255, "u8 max");

decoder.setBuffer(Buffer.from([ 0x00 ]));
assert(decoder.u8() === 0, "u8 zero");

// s16 tests
decoder.setBuffer(Buffer.from([ 0xff, 0x7f ]));
assert(decoder.s16() === 32767, "s16 positive max");

decoder.setBuffer(Buffer.from([ 0x00, 0x80 ]));
assert(decoder.s16() === -32768, "s16 negative max");

decoder.setBuffer(Buffer.from([ 0x00, 0x00 ]));
assert(decoder.s16() === 0, "s16 zero");

// u16 tests
decoder.setBuffer(Buffer.from([ 0xff, 0xff ]));
assert(decoder.u16() === 65535, "u16 max");

decoder.setBuffer(Buffer.from([ 0x00, 0x00 ]));
assert(decoder.u16() === 0, "u16 zero");

// s32 tests
decoder.setBuffer(Buffer.from([ 0xff, 0xff, 0xff, 0x7f ]));
assert(decoder.s32() === 2147483647, "s32 positive max");

decoder.setBuffer(Buffer.from([ 0x00, 0x00, 0x00, 0x80 ]));
assert(decoder.s32() === -2147483648, "s32 negative max");

decoder.setBuffer(Buffer.from([ 0x00, 0x00, 0x00, 0x00 ]));
assert(decoder.s32() === 0, "s32 zero");

// u32 tests
decoder.setBuffer(Buffer.from([ 0xff, 0xff, 0xff, 0xff ]));
assert(decoder.u32() === 4294967295, "u32 max");

decoder.setBuffer(Buffer.from([ 0x00, 0x00, 0x00, 0x00 ]));
assert(decoder.u32() === 0, "u32 zero");

function assert(condition, message)
{
	if (!condition) {
		throw "❌" + message || "Assertion failed";
	} else {
		console.log("✅" + message);
	}
}
