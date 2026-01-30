/*
 * CHESTER Scale - LoRaWAN Payload Decoder
 *
 * Compatible with:
 * - ChirpStack v4 (decodeUplink)
 * - ChirpStack v3 (Decode)
 * - The Things Network (Decoder)
 *
 * Payload structure (26 bytes):
 * Header byte: BIT(0)=battery, BIT(1)=accel, BIT(2)=therm, BIT(6)=scale
 */

var cursor = 0;
var buffer;

// ChirpStack v4
function decodeUplink(input) {
    cursor = 0;
    buffer = input.bytes || input;

    var data = {};

    var header = u8();

    if (header & BIT(0)) {
        decode_batt(data);
    }

    if (header & BIT(1)) {
        decode_accel(data);
    }

    if (header & BIT(2)) {
        decode_therm(data);
    }

    if (header & BIT(6)) {
        decode_scale(data);
    }

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
        data.therm_temperature = data.therm_temperature / 100;
    }
}

function decode_scale(data) {
    data.scale = {};

    var channel_mask = u8();

    data.scale.channel_a1_active = (channel_mask & BIT(0)) !== 0;
    data.scale.channel_a2_active = (channel_mask & BIT(1)) !== 0;
    data.scale.channel_b1_active = (channel_mask & BIT(2)) !== 0;
    data.scale.channel_b2_active = (channel_mask & BIT(3)) !== 0;

    data.scale.raw_a1 = s32();
    data.scale.raw_a2 = s32();
    data.scale.raw_b1 = s32();
    data.scale.raw_b2 = s32();

    // Mark invalid values as null (INT32_MAX = 0x7FFFFFFF)
    if (data.scale.raw_a1 === 0x7FFFFFFF) {
        data.scale.raw_a1 = null;
    }
    if (data.scale.raw_a2 === 0x7FFFFFFF) {
        data.scale.raw_a2 = null;
    }
    if (data.scale.raw_b1 === 0x7FFFFFFF) {
        data.scale.raw_b1 = null;
    }
    if (data.scale.raw_b2 === 0x7FFFFFFF) {
        data.scale.raw_b2 = null;
    }
}

function BIT(index) {
    return (1 << index);
}

function u8() {
    var value = buffer.slice(cursor)[0];
    cursor += 1;
    return value;
}

function s8() {
    var value = buffer.slice(cursor)[0];
    cursor += 1;
    return value > 0x7F ? value - 0x100 : value;
}

function u16() {
    var value = buffer.slice(cursor);
    value = value[0] | (value[1] << 8);
    cursor += 2;
    return value;
}

function s16() {
    var value = buffer.slice(cursor);
    value = value[0] | (value[1] << 8);
    cursor += 2;
    return value > 0x7FFF ? value - 0x10000 : value;
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
    return value | 0;
}

// For testing
function setBuffer(buf) {
    buffer = buf;
    cursor = 0;
}

if (typeof module !== "undefined") {
    module.exports = {
        decodeUplink,
        Decoder,
        Decode,
        u8,
        s8,
        u16,
        s16,
        u32,
        s32,
        setBuffer
    };
}
