var cursor = 0;
var buffer;

function decodeUplink(input) {
    var bytes = input.bytes;
    buffer = bytes;

    var data = {};

    var header = u16();

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

    if ((header & 0x0002) !== 0) {
        data.orientation = u8();

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    if ((header & 0x0004) !== 0) {
        data.therm_temperature = s16();

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

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

    if ((header & 0x0010) !== 0) {
        data.backup = {}

        data.backup.line_voltage = u16();
        data.backup.battery_voltage = u16();
        data.backup.backup_state = u8() !== 0 ? "connected" : "disconnected";

        if (data.backup.line_voltage === 0x7fff) {
            data.backup.line_voltage = null;
        } else {
            data.backup.line_voltage = data.backup.line_voltage / 1000;
        }

        if (data.backup.battery_voltage === 0x7fff) {
            data.backup.battery_voltage = null;
        } else {
            data.backup.battery_voltage = data.backup.battery_voltage / 1000;
        }
    }

    var analog_channels = [];

    for (var i = 0; i < 4; i++) {
        if ((header & (0x0020 << i)) !== 0) {
            var channel = {};
            channel.channel = i + 1;

            var mean_avg = s32();
            var rms_avg = s32();

            channel.measurements = {};

            if (mean_avg === 0x7fffffff) {
                channel.measurements.mean_avg = null;
            } else {
                channel.measurements.mean_avg = mean_avg / 1000;
            }

            if (rms_avg === 0x7fffffff) {
                channel.measurements.rms_avg = null;
            } else {
                channel.measurements.rms_avg = rms_avg / 1000;
            }

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
