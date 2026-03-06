var cursor = 0;
var buffer;

// Chirpstack v4
function decodeUplink(input) {
    cursor = 0;
    buffer = input.bytes || input;

    var data = {};

    var header = u16();

    // BIT(0) - BATT
    if ((header & 0x0001) !== 0) {
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

    // BIT(1) - ACCEL
    if ((header & 0x0002) !== 0) {
        data.orientation = u8();

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    // BIT(2) - THERM
    if ((header & 0x0004) !== 0) {
        data.therm_temperature = s16();

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    // BIT(3) - W1_THERM
    if ((header & 0x0008) !== 0) {
        data.w1_thermometers = [];

        var count = u8();

        for (var i = 0; i < count; i++) {
            var t = s16();

            if (t === 0x7fff) {
                t = null;
            } else {
                t = t / 100;
            }

            data.w1_thermometers.push(t);
        }
    }

    // BIT(4) - BACKUP
    if ((header & 0x0010) !== 0) {
        data.backup = {};

        data.backup.line_voltage = u16();
        data.backup.battery_voltage = u16();
        data.backup.backup_state = u8() !== 0 ? "connected" : "disconnected";

        if (data.backup.line_voltage === 0xffff) {
            data.backup.line_voltage = null;
        } else {
            data.backup.line_voltage = data.backup.line_voltage / 1000;
        }

        if (data.backup.battery_voltage === 0xffff) {
            data.backup.battery_voltage = null;
        } else {
            data.backup.battery_voltage = data.backup.battery_voltage / 1000;
        }
    }

    // BIT(5-8) - CHANNELS 1-4
    var analog_channels = [];

    for (var i = 0; i < 4; i++) {
        if ((header & (0x0020 << i)) !== 0) {
            var channel = {};
            channel.channel = i + 1;
            channel.raw_rms = float16();
            channel.raw_mean = float16();
            channel.calibrated = float16();
            analog_channels.push(channel);
        }
    }

    if (analog_channels.length > 0) {
        data.analog_channels = analog_channels;
    }

    return {
        data: data
    };
}

// The Things Network (TTS)
function Decoder(bytes, port) {
    var decoded = decodeUplink({ bytes: bytes, fPort: port });
    return decoded.data;
}

// Chirpstack v3
function Decode(fPort, bytes) {
    var decoded = decodeUplink({ fPort: fPort, bytes: bytes });
    return decoded.data;
}

function s8() {
    var value = buffer.slice(cursor);
    value = value[0];
    if ((value & (1 << 7)) > 0) {
        value = (~value & 0xff) + 1;
        value = -value;
    }
    cursor = cursor + 1;
    return value;
}

function u8() {
    var value = buffer.slice(cursor);
    value = value[0];
    cursor = cursor + 1;
    return value;
}

function s16() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8;
    if ((value & (1 << 15)) > 0) {
        value = (~value & 0xffff) + 1;
        value = -value;
    }
    cursor = cursor + 2;
    return value;
}

function u16() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8;
    cursor = cursor + 2;
    return value;
}

function u32() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8 | value[2] << 16 | value[3] << 24;
    cursor = cursor + 4;
    return value >>> 0; // convert to unsigned 32-bit
}

function s32() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8 | value[2] << 16 | value[3] << 24;
    if ((value & (1 << 31)) > 0) {
        value = (~value & 0xffffffff) + 1;
        value = -value;
    }
    cursor = cursor + 4;
    return value;
}

function float16() {
    var uint16 = u16();

    if (uint16 === 0x7E00) return null; // NaN marker

    var sign = (uint16 >> 15) & 0x1;
    var exp = (uint16 >> 10) & 0x1F;
    var mant = uint16 & 0x3FF;

    if (exp === 0) {
        // Zero or denormalized
        return (sign ? -1 : 1) * Math.pow(2, -14) * (mant / 1024);
    } else if (exp === 31) {
        // Infinity or NaN
        return mant ? null : (sign ? -Infinity : Infinity);
    }

    return (sign ? -1 : 1) * Math.pow(2, exp - 15) * (1 + mant / 1024);
}

// Used for testing
function setBuffer(buf) {
    buffer = buf;
    cursor = 0;
}

if (typeof module !== "undefined") {
    module.exports = {
        decodeUplink,
        Decoder,
        Decode,
        s8,
        u8,
        s16,
        u16,
        s32,
        u32,
        float16,
        setBuffer
    };
}
