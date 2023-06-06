var cursor = 0;
var buffer;

// Uncomment for ChirpStack:
function Decode(port, bytes, variables) {

// Uncomment for The Things Network:
// function Decoder(port, bytes) {

    buffer = bytes;

    var data = {};

    var header = u8();

    if ((header & 0x01) !== 0) {
        data.voltage_rest = u16();
        data.voltage_load =  u16();
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

    if ((header & 0x02) !== 0) {
        data.orientation = u8();

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    if ((header & 0x04) !== 0) {
        data.therm_temperature = s16();

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    if ((header & 0x10) !== 0) {
        data.hygro_temperature = s16();
        data.hygro_humidity = u8();

        if (data.hygro_temperature === 0x7fff) {
            data.hygro_temperature = null;
        } else {
            data.hygro_temperature = data.hygro_temperature / 100;
        }

        if (data.hygro_humidity === 0xff) {
            data.hygro_humidity = null;
        } else {
            data.hygro_humidity = data.hygro_humidity / 2;
        }
    }

    if ((header & 0x20) !== 0) {
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

    if ((header & 0x40) !== 0) {
        data.rtd_thermometers = [];

        var count = u8();

        for (var i = 0; i < count; i++) {
            var t = s16();

            if (t === 0x7fff) {
                t = null;
            } else {
                t = t / 100;
            }

            data.rtd_thermometers.push(t);
        }
    }

    return data;
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
    return value;
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

if (false) {
    var hex = "00000020026609ff7fff97260200e712691298b700000000000000000000000000000000000000000000"
    var buf = Buffer.from(hex, 'hex')
    console.log(JSON.stringify(Decode(1, buf, 0)));
}
