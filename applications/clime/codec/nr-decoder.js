if (msg.payload.applicationName !== 'ember-application-chester-clime') {
    return null;
}

if (typeof msg.payload.data === 'string') {
    msg.payload.data = decode(Buffer.from(msg.payload.data, 'base64'));
}

return msg;

function decode(buffer) {
    let data = {};

    let offset = 0;

    let header = buffer.readUInt8(0);
    offset += 1;

    if ((header & 0x01) !== 0) {
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

    if ((header & 0x02) !== 0) {
        data.orientation = buffer.readUInt8(offset);
        offset += 1;

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    if ((header & 0x04) !== 0) {
        data.therm_temperature = buffer.readInt16LE(offset);
        offset += 2;

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    if ((header & 0x10) !== 0) {
        data.hygro_temperature = buffer.readInt16LE(offset);
        offset += 2;

        data.hygro_humidity = buffer.readUInt8(offset);
        offset += 1;

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

    if ((header & 0x40) !== 0) {
        data.rtd_thermometers = [];

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

            data.rtd_thermometers.push(t);
        }
    }

    if ((header & 0x80) !== 0) {
        let line = buffer.readUInt8(offset);
        offset += 1;

        data.dc_line = (line != 0);
    }

    return data;
}
