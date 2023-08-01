if (msg.payload.applicationName !== 'application-chester-current') {
    return null;
}

if (typeof msg.payload.data === 'string') {
    msg.payload.data = decode(Buffer.from(msg.payload.data, 'base64'));
}

return msg;

function decode(buffer) {
    let data = {};

    let offset = 0;

    let header = buffer.readUInt16LE(0);
    offset += 2;

    if ((header & 0x0001) !== 0) {
        data.voltage_rest = buffer.readUInt16LE(offset);
        offset += 2;

        data.voltage_load = buffer.readUInt16LE(offset);
        offset += 2;

        data.current_load = buffer.readUInt8(offset);
        offset += 1;

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
        data.orientation = buffer.readUInt8(offset);
        offset += 1;

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    if ((header & 0x0004) !== 0) {
        data.therm_temperature = buffer.readInt16LE(offset);
        offset += 2;

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    if ((header & 0x0008) !== 0) {
        data.w1_thermometers = [];

        let count = buffer.readUInt8(offset);
        offset += 1;

        for (let i = 0; i < count; i++) {
            let t = buffer.readInt16LE(offset);
            offset += 2;

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

        data.backup.line_voltage = buffer.readUInt16LE(offset);
        offset += 2;

        data.backup.battery_voltage = buffer.readUInt16LE(offset);
        offset += 2;

        data.backup.backup_state = buffer.readUInt8(offset) !== 0 ? "connected" : "disconnected";
        offset += 1;

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

    let analog_channels = [];

    for (let i = 0; i < 4; i++) {
        if ((header & (0x0020 << i)) !== 0) {
            let channel = {};
            channel.channel = i + 1;

            let mean_avg = buffer.readInt32LE(offset);
            offset += 4;
            let rms_avg = buffer.readInt32LE(offset);
            offset += 4;

            channel.measurements = {};

            if (mean_avg === 0x7fffffff) {
                channel.measurements.mean_avg = null
            } else {
                channel.measurements.mean_avg = mean_avg / 1000;
            }

            if (rms_avg === 0x7fffffff) {
                channel.measurements.rms_avg = null
            } else {
                channel.measurements.rms_avg = rms_avg / 1000;
            }

            analog_channels.push(channel);
        }
    }

    if (analog_channels.length > 0) {
        data.analog_channels = analog_channels;
    }

    return data;
}
